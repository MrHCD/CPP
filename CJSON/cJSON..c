#if !defined(_CRT_SECURE_NO_DEPRECATE)&&defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef _GNUC_
#pragma GCC visibility push(default)
#endif

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning(disable :4001)
#endif

#include<string.h>
#include<stdio.h>
#include<math.h>
#include<stdlib.h>
#include<limits.h>
#include<ctype.h>

#ifdef ENABLE_LOCALES
#include<locale.h>
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#ifdef _GNUC_
#pragma GCC visibility pop
#endif

#include"cJSON.h"

#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

typedef struct
{
    const unsigned char *json;
    size_t position;
}error;

static error global_error={NULL,0};

CJSON_PUBLIC(const char*) cJSON_GetErrorPtr(void){
    return (const char*)(global_error.json+global_error.position);
}

CJSON_PUBLIC(char *)cJSON_GetStringValue(cJSON *item){
    if(!cJSON_IsString(item)){
        return NULL;
    }
    return item->valuestring;
}

#if (CJSON_VERSION_MAJOR!=1)||(CJSON_VERSION_MINOR!=7)||(CJSON_VERSION_PATCH!=11)
    #error cJSON.h and cJSON.c have different versions make sure that both have __RETURN_POLICY_SAME
#endif

CJSON_PUBLIC(const char*)cJSON_Version(void){
    static char version[15];
    sprintf(version,"%i,%i,%i",CJSON_VERSION_MAJOR,CJSON_VERSION_MINOR,CJSON_VERSION_PATCH);
    return version;
}

static int case_insensitive_strcmp(const unsigned char* string1,const unsigned char *string2){
    if((string1==NULL)||(string2==NULL)){
        return 1;
    }

    if(string1==string2){
        return 0;
    }

    for(;tolower(*string1)==tolower(*string2);(void)string1++,string2++){
        if(*string1=='\0'){
            return 0;
        }
    }

    return tolower(*string1)-tolower(*string2);
}

typedef struct internal_hooks
{
    void *(CJSON_CDECL *allocate)(size_t size);
    void (CJSON_CDECL *deallcoate)(void *pointer);
    void *(CJSON_CDECL *realloccate)(void *pointer,size_t size);
}internal_hooks;

#if defined(_MSC_VER)
static void *CJSON_CDECL internal_malloc(size_t size){
    return malloc(size);
}
static void CJSON_CDECL internal_free(void *pointer){
    free(pointer);
}
static void *CJSON_CDECL internal_realloc(void *pointer,size_t size){
    return realloc(pointer,size);
}
#else
#define internal_malloc malloc
#define internal_free free
#define internal_realloc realloc

#endif

//strlen of character literals resolved at compile time
#define static_strlen(string_literal) (sizeof(string_literal)-sizeof(""))

static internal_hooks global_hooks={internal_malloc,internal_free,internal_realloc};

//copy a new string to a new placement
static unsigned char* cJSON_strdup(const unsigned char* string,const internal_hooks*const hooks){
    size_t length=0;
    unsigned char *copy=NULL;

    if(string ==NULL){
        return NULL;
    }

    length=strlen((const char* )string)+sizeof("");
    copy=(unsigned char*)hooks->allocate(length);
    if(copy==NULL){
        return NULL;
    }
    memcpy(copy,string,length);
    return copy;
}

CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks *hooks){
    if(hooks=NULL){
        //Reset hook
        global_hooks.allocate=malloc;
        global_hooks.deallcoate=free;
        global_hooks.realloccate=realloc;
        return;
    }

    global_hooks.allocate=malloc;
    if(hooks->malloc_fn!=NULL){
        global_hooks.allocate=hooks->malloc_fn;
    }

    //use realloc only if both free and malloc are used
    global_hooks.realloccate=NULL;
    if((global_hooks.allocate==malloc)&&(global_hooks.deallcoate==free)){
        global_hooks.realloccate=realloc;
    }
}

static cJSON *cJSON_NEW_Item(const internal_hooks *const hooks){
    cJSON *node=(cJSON*)hooks->allocate(sizeof(cJSON));
    if(node){
        memset(node,'\0',sizeof(cJSON));
    }
    return node;
}


//delete a cJSON structure
CJSON_PUBLIC(void)cJSON_Delete(cJSON*item){
    cJSON *next=NULL;
    while(item!=NULL){
        next=item->next;
        if(!(item->type&cJSON_IsReference)&&(item->child!=NULL)){
            cJSON_Delete(item->child);
        }
        if(!(item->type&cJSON_IsReference)&&(item->valuestring!=NULL)){
            global_hooks.deallcoate(item->valuestring);
        }
        if(!(item->type&cJSON_StringIsConst)&&(item->string!=NULL)){
            global_hooks.deallcoate(item->string);
        }
        global_hooks.deallcoate(item);
        item->next;
    }
}


static unsigned char get_decimal_point(void){
#ifdef ENABLE_LOCALES
    struct lconv *lconv=localeconv(); //a struct that include the information of number and currency
    return (unsigned char)lconv->decimal_pointer[0];
#else 
    return '.';
#endif
}

typedef struct{
    const unsigned char* content;
    size_t length;
    size_t offset;
    size_t depth; //How deeply nested(in arrays/objects) is the input at the current offset
    internal_hooks hooks;
}parse_buffer;

//check if the given size is left to read in a given parse buffer (starting with 1)
#define can_read(buffer,size)  ((buffer!=NULL)&&(((buffer)->offset+size)<=(buffer)->length))
//check if the buffer can be accessed at the given index (starting with 1)
#define can_access_at_index(buffer,index) ((buffer!=NULL)&&(((buffer)->offset+index)<(buffer)->length))
#define cannot_access_at_index(buffer,index) (!can_access_at_index(buffer,index))
//get a pointer to the buffer at the position
#define buffer_at_offset(buffer) ((buffer)->content+(buffer)->offset)

//parse the input text to generate a number, and populate the result into item
static cJSON_bool parse_number(cJSON *const item,parse_buffer*const input_buffer){
    double number=0;
    unsigned char*after_end=NULL;
    unsigned char number_c_string[64];
    unsigned char decimal_pointer=get_decimal_point();
    size_t i=0;

    if((input_buffer)==NULL||(input_buffer->content)==NULL){
        return false;
    }

    /*copy the number into a temporary buffer and replace '.' with the decimal point
    of the current locale(for strtod)
    this also takes care of '\0' not necessarily being available for marking the end of input
     * */

    for(i=0;(i<(sizeof(number_c_string)-1))&&can_access_at_index(input_buffer,1);i++){
        switch (buffer_at_offset(input_buffer)[i])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '+':
        case '-':
        case 'e':
        case 'E':
            number_c_string[i]=buffer_at_offset(input_buffer)[i];
            break;
        case '.':
            number_c_string[i]=decimal_pointer;
            break;
        
        default:
            goto loop_end;
        }
    }
loop_end:
    number_c_string[i]='\0';
    //如果after_end不为NULL，则将遇到的不符合条件而终止的ntr中的字符指针由endptr传回
    number=strtod((const char*)number_c_string,(char**)&after_end);
    if(number_c_string==after_end){
        return false;//parse_error
    }

    item->valuedouble=number;

    if(number>=INT_MAX){
        item->valueint=INT_MAX;
    }
    else if(number<=(double)INT_MIN){
        item->valueint=INT_MIN;
    }
    else{
        item->valueint=(int)number;
    }

    item->type=cJSON_Number;

    input_buffer->offset+=(size_t)(after_end-number_c_string);
    return true;
}

CJSON_PUBLIC(double)cJSOn_setNumberHelper(cJSON*object,double number){
    if(number>=INT_MAX){
        object->valueint=INT_MAX;;
    }else if(number<=(double)INT_MIN){
        object->valueint=INT_MIN;
    }else{
        object->valueint=(int)number;
    }

    return object->valuedouble=number;
}

typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth;
    cJSON_bool noalloc;
    cJSON_bool format;//is this print a formatted print
    internal_hooks hooks;
}printbuffer;

static unsigned char* ensure(printbuffer *const p,size_t needed){
    unsigned char *newbuffer=NULL;
    size_t newsize=0;

    if((p==NULL)||(p->buffer==NULL)){
        return NULL;
    }

    if((p->length>0)&&(p->offset>=p->length)){
        return NULL;
    }

    if(needed>INT_MAX){
        return NULL;
    }

    needed+=p->offset+1;

    if(needed<=p->length){
        return p->buffer+p->offset;
    }

    if(p->noalloc){
        return NULL;
    }

    //calculate new buffer szie

   if(needed>(INT_MAX/2)){
        if(needed <=INT_MAX){
        newsize=INT_MAX;
        }
        else
        {
        return NULL;
        }
   }else
   {
       newsize=needed*2;
   }

   if(p->hooks.realloccate!=NULL){
       newbuffer=(unsigned char*)p->hooks.realloccate(p->buffer,newsize);
       if(newbuffer==NULL){
           p->hooks.deallcoate(p->buffer);
           p->length=0;
           p->buffer=NULL;

           return NULL;
       }

       if(newbuffer){
            memcpy(newbuffer,p->buffer,p->offset+1);
       }

       p->hooks.deallcoate(p->buffer);

   }
   p->length=newsize;
   p->buffer=newbuffer;

   return newbuffer+p->offset;
      
}

//Render the number nicely from the given item into string
static cJSON_bool print_number(const cJSON*const item,printbuffer *const output_buffer){
   unsigned char*output_pointer=NULL;
   double d=item->valuedouble;
   int length=0;
   size_t i=0;
   unsigned char number_buffer[26];
   unsigned char decimal_point=get_decimal_point();
   double test;

   if(output_buffer==NULL){
       return false;
   }

   //this checks for NaN and Infinity
   if((d*0)!=0){
       length=sprintf((char*)number_buffer,"null");
   }
   else{
       //尝试15个小数位数的精度避免不重要的非零数字 
       length=sprintf((char*)number_buffer,"%1.15g",d);

       if((sscanf((char*)number_buffer,"%1g",&test)!=1)||(double)test!=d){
           length=sprintf((char*)number_buffer,"%1.17g",d);
       }
   }

    //sprintf failed or buffer overrun occured
   if((length<0)||(length>(int)(sizeof(number_buffer)-1))){
       return false;
   }

   //reserve appropriate space in the output
   output_pointer=ensure(output_buffer,(size_t)length+sizeof(""));
   if(output_buffer==NULL){
       return false;
   }

   //copy the printed number to the output and replace locale
   //dependent decimal point with '.'
   for(i=0;i<((size_t)length);i++){
       if(number_buffer[i]==decimal_point){
           output_pointer[i]='.';
           continue;
       }

       output_pointer[i]==number_buffer[i];
   }
   output_pointer[i]='\0';
   output_buffer->offset+=(size_t)length;

   return true;

}

//parse 4 digit hexadecimal number
static unsigned parse_hex4(const unsigned char*const input){
    unsigned int h=0;
    size_t i=0;

    for(i=0;i<4;i++){
        if((input[i]>='0')&&input[i]<='9'){
            h+=(unsigned int)input[i]-'0';
        }
        else if((input[i]>='A')&&(input[i]<='F')){
            h+=(unsigned int)10+input[i]-'A';
        }
        else if((input[i]>='a')&&(input[i]<='f')){
            h+=(unsigned int)10+input[i]-'a';
        }
        else{
            return 0;
        }

        if(i<3){
            //左移4位留出半个字节
            h=h<<4;
        }
    }

    return h;
}

//convert a UTF-16 literal to UTF-8
//a literal can be one or two sequence of the form \uXXXX

static unsigned char utf16_literal_to_uft8(const unsigned char*const input_pointer,const unsigned char*const input_end,unsigned char**output_pointer){
    long unsigned int codepoint=0;
    unsigned int first_code=0;
    const unsigned char*first_sequence=input_pointer;
    unsigned char utf8_length=0;
    unsigned char utf8_position=0;
    unsigned char sequence_length=0;
    unsigned char first_byte_mark=0;
    //utf16使用2/4个字节进行储存 （0-FFFF）部分统一使用2个字节来进行储存,字符映射范围（U+0000-U+10FFFF）
    //utf32使用固定的4个字节来进行储存
    //utf8纯在单字节编码，使用1-6个字节进行储存，兼容ASCII,当编码超过一个字节，则设最高比特位为0
    //当编码超过一个字节，则需要几个字节，就在第一个字节最高位开始令连续的几个比特为1，之后的字节最高为10
    if((input_end-first_sequence)<6){
        goto fail;
    }

    first_code=parse_hex4(first_sequence+2);

    //utf16 surrogate pair
    //一个生僻字符需要使用0xffff以上的映射范围，即需要两个码元，也即是4byte
    //第一个码元(前导代理)：0xD800-0xDBFF
    //第二个码元(后尾代理)：0xDC00-0xDFFF
    //单独的前导代理不能够表示字符，必须与后尾代理一起构成一个完整字，即必须是双映射
    if((first_code>=0xDC00)&&(first_code<=0xDFFF)){
        goto fail;
    }

    if((first_code>=0xD800)&&(first_code<=0xDBFF)){
        const unsigned char *second_sequence=first_sequence+6;
        unsigned int second_code=0;
        sequence_length=12;

        if((input_end-second_sequence)<6){
            goto fail;
        }

        if((second_sequence[0]!='\\')||(second_sequence[1]!='u')){
            //missing second half of the surrogate pair
            goto fail;
        }

        second_code=parse_hex4(second_sequence+2);

        if((second_code<0xDC00)||(second_code>0xDFFF)){
            goto fail;
        }
        //calculate the unicode codepoint from the surrogate pair
        codepoint =0x10000+(((first_code&0x3ff)<<10)|(second_code&0x3ff));//映射方式
    }
    else{
        sequence_length=6;
        codepoint=first_code;
    }

    //encode as utf8
    if(codepoint<0x80){
        utf8_length=1;//normal ASCII.encoding 0xxxxxxx
    }
    else if(codepoint<0x800){
        utf8_length=2;  //two bytes encoding 110xxxxx 10xxxxxx
        first_byte_mark=0xc0; //11000000
    }else if(codepoint<0x10000){
        utf8_length=3;//three bytes encoding 1110xxxx 10xxxxxx 10xxxxxx 
        first_byte_mark=0xe0;//11100000
    }else if(codepoint<=0x10FFFF){
        utf8_length=4;//four bytes,encoding 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        first_byte_mark=0xF0; //11110000
    }else{
        goto fail;
    }

    for(utf8_position=(unsigned char)(utf8_length-1);utf8_position>0;utf8_position--){
        (*output_pointer)[utf8_position]=(unsigned char)((codepoint|0x80)&0xbf);
        codepoint>>=6;
    }

    if(utf8_length>1){
        (*output_pointer)[0]=(unsigned char)((codepoint|first_byte_mark)&0xff);
    }
    else{
        (*output_pointer)[0]=(unsigned char)(codepoint&0x7F);
    }
    *output_pointer+=utf8_length;

    return sequence_length;

    

fail:
    return 0;
}


static cJSON_bool parse_string(cJSON*const item,parse_buffer*const input_buffer){
    const unsigned char *input_pointer=buffer_at_offset(input_buffer)+1;
    const unsigned char*input_end=buffer_at_offset(input_buffer)+1;
    unsigned char *output_pointer=NULL;
    unsigned char *output=NULL;

    if(buffer_at_offset(input_buffer)[0]!='\"'){
        goto fail;
    }

    
    //calculate approximate sizeof the output(overestimate)
    size_t allocation_length=0;
    size_t skipped_bytes=0;
    while(((size_t)(input_end-input_buffer->content)<input_buffer->length)&&(*input_end!='\"')){
        if(input_end[0]=='\\'){
            if((size_t)(input_end+1-input_buffer->content)>=input_buffer->length){
                goto fail;
            }
            skipped_bytes++;
            input_end++;
        }
        input_end++;
    }
    if(((size_t)(input_end-input_buffer->content)>=input_buffer->length)||(*input_end!='\"')){
        goto fail;
    }

    allocation_length=(size_t)(input_end-buffer_at_offset(input_buffer))-skipped_bytes;
    output=(unsigned char*)input_buffer->hooks.allocate(allocation_length+sizeof(""));
    if(output==NULL){
        goto fail;
    }


    output_pointer=output;
    while (input_pointer<input_end)
    {
        if(*input_pointer!='\\'){
            *output_pointer++ = *input_pointer++;
        }
        else{
            unsigned char sequence_length=2;
            if((input_end-input_pointer)<1){
                goto fail;
            }
            switch (input_pointer[1])
            {
            case 'b':
                *output_pointer++='\b';
                break;
            case 'f':
                *output_pointer++='\f';
                break;
            case 'n':
                *output_pointer++='\n';
                break;
            case 'r':
                *output_pointer++='\r';
                break;
            case 't':
                *output_pointer++='\t';
                break;
            case '\"':
            case '\\':
            case '/':
                *output_pointer++ = input_pointer[1];
                break;
            case 'u':
            sequence_length=utf16_literal_to_uft8(input_pointer,input_end,&input_buffer);
            if(sequence_length==0)
            {
                goto fail;
            }
                break;
            default:
                goto fail;
            }
            input_pointer+=sequence_length;
         
        }

    }
       *output_pointer='\0';

        item->type=cJSON_String;
        item->valuestring=(char*)output;
        input_buffer->offset=(size_t)(input_end-input_buffer->content);
        input_buffer->offset++;
        return true;
    

fail:
    if(output!=NULL){
        input_buffer->hooks.deallcoate(output);
    }
    if(input_buffer!=NULL){
        input_buffer->offset=(size_t)(input_pointer-input_buffer->content);
    }

    
    return false;

}

//Render the cstring provided to an secaped version that can be printed
static cJSON_bool print_string_ptr(const unsigned char* const input,printbuffer*const output_buffer){
    const unsigned char* input_pointer=NULL;
    unsigned char* output=NULL;
    unsigned char* output_pointer=NULL;
    size_t output_length=0;

    //numbers of additional characters needed for escaping
    //即需要避免的额外的词汇
    size_t escape_characters=0;

    if(output_buffer==NULL){
        return  false;
    }

    //empty string
    if(input==NULL){
        output=ensure(output_buffer,sizeof("\"\""));
        if(output==NULL){
            return false;
        }
        strcpy((char*)output,"\"\"");
        return true;
    }
    
    for(input_pointer=input;*input_pointer;*input_pointer++){
        switch (*input_pointer)
        {
        case '\"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            escape_characters++;
            break;
        
        default:
            //若ASCII值小于32，即无法找到对应的字符类型组成字符串
            if(*input_pointer<32){
                //utf16 escape sequence uXXXX
                escape_characters+=5;
            }
            break;
        }
    }
    //以上确定输出长度
    output_length=(size_t)(input_pointer-input)+escape_characters;

    output=ensure(output_buffer,output_length+sizeof("\"\""));
    if(output==NULL){
        return false;
    }

    //没有字符需要被跳过
    if(escape_characters==0){
        output[0]='\"';
        memcpy(output+1,input,output_length);
        output[output_length+1]='\"';
        output[output_length+2]='\0';
        return true;
    }

    output[0]='\"';
    output_pointer=output+1;

    for(input_pointer=input;*input_pointer!='\0';(void)input_pointer++,output_pointer++){
        if((*input_pointer>31)&&(*input_pointer!='\"')&&(*input_pointer!='\\')){
            *output_pointer=*input_pointer;
        }
        else{
            *output_pointer++ ='\\';
            switch (*input_pointer)
            {
            case '\\':
                *output_pointer='\\';
                break;
            case '\"':
                *output_pointer='\"';
                break;
            case '\b':
                *output_pointer='b';
                break;
            case '\f':
                *output_pointer='f';
                break;
            case '\n':
                *output_pointer='n';
                break;
            case '\r':
                *output_pointer='r';
                break;
            case '\t':
                *output_pointer='t';
                break;
            default:
                sprintf((char*)output_pointer,"u%04x",*input_pointer);
                output_pointer+=4;
                break;
            }
        }
    }

    output[output_length+1]='\"';
    output[output_length+2]='\0';
    return true;
}

static cJSON_bool print_string(const cJSON*const item,printbuffer*const p){
    return print_string_ptr((unsigned*)item->valuestring,p);
}

//Predeclare these prototypes
static cJSON_bool parse_value(cJSON*const item,parse_buffer*const input_buffer);
static cJSON_bool print_value(const cJSON *const item,printbuffer*const output_buffer);
static cJSON_bool parse_array(cJSON*const item,parse_buffer *const input_buffer);
static cJSON_bool print_array(const cJSON*const item,printbuffer*const output_buffer);
static cJSON_bool parse_object(cJSON*const item,parse_buffer*const input_buffer);
static cJSON_bool print_object(const cJSON*const item,printbuffer *const output_buffer);

static parse_buffer *buffer_skip_whitespace(parse_buffer*const buffer){
    if((buffer==NULL)||(buffer->content==NULL)){
        return NULL;
    }
    //将offset指针移动至第一个字符的位置
    while (can_access_at_index(buffer,0)&&(buffer_at_offset(buffer)[0]<=32))
    {
        buffer->offset++;
    }

    if(buffer->offset==buffer->length){
        buffer->offset--;
    }

    return buffer;
    
}

//skip the utf8 bom(byte order mark) if it is at the beginning of a buffer
static parse_buffer *skip_utf8_bom(parse_buffer*const buffer){
    if((buffer==NULL)||(buffer->content==NULL)||(buffer->offset!=0)){
        return NULL;
    }
    //\xEF\xBF\xBB即bom文件的标记，用来指出这个文件是UTF8编码
    if(can_access_at_index(buffer,4)&&(strncmp((const char *)buffer_at_offset(buffer),"\xEF\xBB\xBF",3)==0)){
        buffer->offset+=3;
    }

    return buffer;
}

//Parse an object -create a new root ,and populate
CJSON_PUBLIC(cJSON*) cJSON_ParseWithOpts(const char*value,const char**return_parse_end,cJSON_bool require_null_terminated){
    parse_buffer buffer={0,0,0,0,{0,0,0}};
    cJSON *item=NULL;

    //reset error position
    global_error.json=NULL;
    global_error.position=0;

    if(value==NULL){
        goto fail;
    }

    buffer.content=(const unsigned char*)value;
    buffer.length=strlen((const char*)value)+sizeof("");
    buffer.offset=0;
    buffer.hooks=global_hooks;

    item=cJSON_NEW_Item(&global_hooks);
    if(item==NULL)
    {
        goto fail;
    }

    if(!parse_value(item,buffer_skip_whitespace(skip_utf8_bom(&buffer)))){
        
        goto fail;
    }

    //if we require null-terminated JSON without appended garbage ,skip and then check fo a null terminator
    if(require_null_terminated){
        buffer_skip_whitespace(&buffer);
        if((buffer.offset>=buffer.length)||buffer_at_offset(&buffer)[0]!='\0'){
            goto fail;
        }
    }

    if(return_parse_end){
        *return_parse_end=(const char*)buffer_at_offset(&buffer);
    }

    return item;


fail:
    if(item!=NULL){
        cJSON_Delete(item);
    }
    
    if(value!=NULL){
        error local_error;
        local_error.json=(const unsigned char*)value;
        local_error.position=0;

        if(buffer.offset<buffer.length){
            local_error.position=buffer.offset;
        }
        else if(buffer.length>0){
            local_error.position=buffer.length-1;
        }
        if(return_parse_end!=NULL){
            *return_parse_end=(const char *)local_error.json+local_error.position;
        }
        global_error=local_error;
    }

    return NULL;


}


CJSON_PUBLIC(cJSON*)cJSON_Parse(const char *value){
    return cJSON_ParseWithOpts(value,0,0);
}

#define cjson_min(a,b) ((a<b)?a:b)

static unsigned char*print(const cJSON* const item,cJSON_bool format,const internal_hooks*const hooks){
    static const size_t default_buffer_size=256;
    printbuffer buffer[1];
    unsigned char*printed=NULL;
    //memset 将一段内存空间全部设置为某个字符
    memset(buffer,0,sizeof(buffer));
    
    //create buffer
    buffer->buffer=(unsigned char*)hooks->allocate(default_buffer_size);
    buffer->length=default_buffer_size;
    buffer->format=format;
    buffer->hooks=*hooks;
    if(buffer->buffer==NULL){
        goto fail;
    }

    if(!print_value(item,buffer)){
        goto fail;
    }
    update_offset(buffer);

    if(hooks->realloccate!=NULL){
        printed=(unsigned char*)hooks->realloccate(buffer->buffer,buffer->offset+1);
        if(printed==NULL){
            goto fail;
        }
        buffer->buffer=NULL;
    }
    else
    {
        printed=(unsigned char*)hooks->allocate(buffer->offset+1);
        if(printed==NULL){
            goto fail;
        }
        memcpy(printed,buffer->buffer,cjson_min(buffer->length,buffer->offset+1));
        printed[buffer->offset]='\0';

        hooks->deallcoate(buffer->buffer);
    }

    return printed;
    

fail:
    if(buffer->buffer!=NULL){
        hooks->deallcoate(buffer->buffer);
    }

    if(printed!=NULL){
        hooks->deallcoate(printed);
    }
    return NULL;                                           
}

CJSON_PUBLIC(char*)cJSON_Print(const cJSON*item){
    return (char *)print(item,true,&global_hooks);
}

CJSON_PUBLIC(char*)cJSON_PrintUnformatted(const cJSON*item){
    return (char*)print(item,false,&global_hooks);
}

CJSON_PUBLIC(char*)cJSON_PrintBuffered(const cJSON *item,int prebuffer,cJSON_bool fmt){
    printbuffer p={0,0,0,0,0,0,{0,0,0}};
    if(prebuffer<0){
        return NULL;
    }
    p.buffer=(unsigned char*)global_hooks.allocate((size_t)prebuffer);
    if(!p.buffer){
        return NULL;
    }

    p.length=(size_t)prebuffer;
    p.offset=0;
    p.noalloc=false;
    p.format=fmt;
    p.hooks=global_hooks;

    if(!print_value(item,&p)){
        global_hooks.deallcoate(p.buffer);
        return NULL;
    }

    return (char*)p.buffer;
}

CJSON_PUBLIC(cJSON_bool)cJSON_PrintPreallocated(cJSON*item,char *buf,const int len,const cJSON_bool fmt){
     printbuffer p={0,0,0,0,0,0,{0,0,0}};
     if((len<0)||(buf==NULL)){
         return false;
     }

     p.buffer=(unsigned char*)buf;
     p.length=(size_t)len;
     p.offset=0;
     p.format=fmt;
     p.hooks=global_hooks;

     return print_value(item,&p);
}

//Parser core -when encountering text,process appropriately
static cJSON_bool parse_value(cJSON*const item,parse_buffer *const input_buffer){
    if((input_buffer==NULL)||(input_buffer->content==NULL)){
        return false;
    }

    //parse the different types of values
    //NULL
    if(can_read(input_buffer,4)&&(strncmp((const char*)buffer_at_offset(input_buffer),"null",4)==0)){
        item->type=cJSON_NULL;
        input_buffer->offset+=4;
        return true;
    }
    
}



