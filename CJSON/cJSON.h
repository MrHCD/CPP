#ifndef cJSON_h
#define cJSON_h

#ifdef _cplusplus
extern "c"{
#endif

#if !defined(_WINDOWS_)&&(defined(WIN32)||defined(WIN64)||defined(_MSC_VER)||defined(_WIN32_))
#define _WINDOWS_
#endif

#ifdef _WINDOWS_
#define CJSON_CDECL _cdecl
#define CJSON_STDCALL _stdcall

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(CJSON_HIDE_SYMBOLS)&&!defined(CJOSN_IMPORT_SYMBOLS)&&!defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_EXPORT_SYMBOLS
#endif

#if defined(CJSON_HIDE_SYMBOLS)
#define CJSON_PUBLIC(type) type CJSON_STDCALL
#elif defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_PUBLIC(type) _declspec(dllexport) type CJSON_STDCALL
#elif defined(CJSON_IMPORT_SYMBOLS)
#define CJSON_PUBLIC(type) _declspec(dllimport) type CJSON_STDCALL
#endif
#else
#define CJSON_CDECL
#define CJSON_STDCALL

#if (defined(_GNUC_)||defined(_SUNPRO_CC)||defined(_SUNPRO_C))&&defined(CJSON_API_VISIBILITY)
#define CJSON_PUBLIC(type) _attribute_((visibility("default"))) type
#else
#define CJSON_PUBLIC(type) type
#endif
#endif

//项目版本
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 11

#include<stddef.h>

//CJSON TYPES:
#define cJSON_Invalid (0)
#define cJSON_False   (1<<0)
#define cJSON_True    (1<<1)
#define cJSON_NULL    (1<<2)
#define cJSON_Number  (1<<3)
#define cJSON_String  (1<<4)
#define cJSON_Array   (1<<5)
#define cJSON_Object  (1<<6)
#define cJSON_Raw     (1<<7)  //raw json

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

typedef struct  cJSON
{
     /* next/prev allow you to walk array/object chains. 
     Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
     struct cJSON *next;
     struct cJSON *prev;
    /* An array or object item will have a child pointer pointing to 
    a chain of the items in the array/object. */
    struct cJSON *child;

    int type;

     /* The item's string, if type==cJSON_String  and type == cJSON_Raw */    
    char *valuestring;
    /*writing to valueint is deprecated, use cJSON_setNumberValue instead*/
    int valueint;
    /*the item's number ,if type==cJSON_number*/
    double valuedouble;

    /*the item's name string ,if this item is child of ,or is in the
    list of submitems of an object*/
    char *string ;
}cJSON;

typedef struct cJSON_Hooks
{
     /* malloc/free are CDECL on Windows regardless of the default calling convention of the compiler,
      so ensure the hooks allow passing those functions directly. */
      void *(CJSON_CDECL *malloc_fn)(size_t sz);
      void (CJSON_CDECL *free_fn)(void *ptr);
}cJSON_Hooks;

typedef int cJSON_bool;

/* Limits how deeply nested arrays/objects can be before cJSON rejects to parse them.
 * This is to prevent stack overflows.*/
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/*retrurn the version of cJSON as string */
CJSON_PUBLIC(const char*) cJSON_Version(void);

/*Supply malloc realloc and free functions to Cjson*/
CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks *hooks);

/* Memory Management: the caller is always responsible to free the results from
all variants of cJSON_Parse (with cJSON_Delete) and 
cJSON_Print (with stdlib free, cJSON_Hooks.free_fn, or cJSON_free as appropriate). 
The exception is cJSON_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a cJSON object you can interrogate. */
CJSON_PUBLIC(cJSON*)cJSON_Parse(const char *value);

CJSON_PUBLIC(cJSON*)cJSON_ParseWithOpts(const char *value,const char **return_parse_end,cJSON_bool require_null_terminated);

CJSON_PUBLIC(char *)cJSON_Print(const cJSON *item);

CJSON_PUBLIC(char*)cJSON_PrintUnformatted(const cJSON *item);

CJSON_PUBLIC(char*)cJSON_PrintBuffered(const cJSON *item,int prebuffer,cJSON_bool fmt);

CJSON_PUBLIC(cJSON_bool)cJSON_PrintPreallocated(cJSON *item,char *buffer,const int length,const cJSON_bool format);

CJSON_PUBLIC(void)cJSON_Delete(cJSON *c);

CJSON_PUBLIC(int)cJSON_GetArraySize(const cJSON *array);

CJSON_PUBLIC(cJSON*)cJSON_GetArrayItem(const cJSON*array,int index);

CJSON_PUBLIC(cJSON*)cJSON_getObjectItem(const cJSON* const object,const char*const string);

CJSON_PUBLIC(cJSON*)cJSON_getObjectItemCaseSensitive(const cJSON*const object,const char* const string);

CJSON_PUBLIC(cJSON_bool)cJSON_HasObjectItem(const cJSON* object,const char*string);

CJSON_PUBLIC(const char*)cJSON_GetErrorPtr(void);

CJSON_PUBLIC(char*)cJSON_GetStringValue(cJSON*item);

//check the type of an item
CJSON_PUBLIC(cJSON_bool)cJSON_ISInvalid(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsFalse(const cJSON*const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsTrue(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsBool(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsNULL(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsNumber(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsString(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsArray(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsObject(const cJSON* const item);
CJSON_PUBLIC(cJSON_bool)cJSON_IsRaw(const cJSON* const item);

//create item of the appropriate type
CJSON_PUBLIC(cJSON*) cJSON_CreateNull(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateTrue(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateFalse(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateBool(cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_CreateNumber(double num);
CJSON_PUBLIC(cJSON*) cJSON_CreateString(const char*string );
CJSON_PUBLIC(cJSON*) cJSON_CreateRaw(const char*raw);
CJSON_PUBLIC(cJSON*) cJSON_CreateArray(void);
CJSON_PUBLIC(cJSON*) cJSON_CreateObject(void);

//Create a string where valuestring refrences a string so 
//it will not be freed by cJSON_Delete
CJSON_PUBLIC(cJSON*) cJSON_CreateStringReference(const char*string);
CJSON_PUBLIC(cJSON*) cJSON_CreateArrayReference(const cJSON*child);
CJSON_PUBLIC(cJSON*) cJSON_CreateObjectReference(const cJSON*child);

//create an array of count items
CJSON_PUBLIC(cJSON*)cJSON_CreateFloatArray(const float *numbers,int count);
CJSON_PUBLIC(cJSON*)cJSON_CreateDoubleArray(const double *number,int count);
CJSON_PUBLIC(cJSON*)cJSON_CreateIntArray(const int *number,int count);
CJSON_PUBLIC(cJSON*)cJSON_CreateStringArray(const char *strings,int count);

//append item to the specified array/object
CJSON_PUBLIC(void) cJSON_AddItemToArray(cJSON *array,cJSON *item);
CJSON_PUBLIC(void) cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *Item);
CJSON_PUBLIC(void) cJSON_AddItemToObjectCS(cJSON *object,const char*string,cJSON*item);
CJSON_PUBLIC(void) cJSON_AddItemRefernceToArray(cJSON *array,cJSON *item);
CJSON_PUBLIC(void) cJSON_AddItemRefernceToObject(cJSON *object,const char*string,cJSON *item);

//Remove/Deatch item from Arrays/objects
CJSON_PUBLIC(cJSON*) cJSON_DetachItemViaPointer(cJSON*parent,cJSON*const item);
CJSON_PUBLIC(cJSON*) cJSON_DetachItemFromArray(cJSON*array,int which);
CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON*array,int which);
CJSON_PUBLIC(cJSON*) cJSON_DetachItemFromObject(cJSON*object,const char* string);
CJSON_PUBLIC(cJSON*) cJSON_DetachItemFromObjectCaseSensitive(cJSON*object,const char *string);
CJSON_PUBLIC(void) cJSON_DeleteItemFromObject(cJSON*object,const char*string);
CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON*Object,const char*string);

//Update array items
CJSON_PUBLIC(void) cJSON_InsertItemInArray(cJSON*array,int which,cJSON*newitem);
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON *const parent,cJSON*const item,cJSON*replacement);
CJSON_PUBLIC(void) cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newItem);
CJSON_PUBLIC(void) cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON*newitem);
CJSON_PUBLIC(void) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object,const char*string,cJSON*newItem);

//Duplicate a cJSON item
CJSON_PUBLIC(cJSON*) cJSON_Duplicate(const cJSON*item,cJSON_bool recurse);

CJSON_PUBLIC(cJSON*) cJSON_Compare(const cJSON* const a,const cJSON*const b,const cJSON_bool case_sensitive);

CJSON_PUBLIC(void) cJSON_Minify(char *json);

//creating and adding items to an object at the same time
//they return the added item or null on failure
CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON*const object,const char*const name);
CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON*const object,const char*const name);
CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON*const object,const char*const name);
CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON*const object,const char*const name,const cJSON_bool boolean);
CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON*const object,const char*const name,double number);
CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON*const object,const char*const name,const char *const string);
CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON*const object,const char*const name,const char*const raw);
CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON*const object,const char*const name);
CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON*const object,const char*const name);

#define cJSON_SetIntValue(object,number)((object)?(object)->valueint=(object)->valuedouble=(number):(number))

CJSON_PUBLIC(double)cJSOn_setNumberHelper(cJSON *object,double number);
#define cJSON_setNumberValue(object,numeber)((oubject!=NULL)?cJSON_SetNumberHelper(object, (double)number) : (number))

CJSON_PUBLIC(void *)cJSON_malloc(size_t size);
CJSON_PUBLIC(void) cJSON_free(void *object);

#ifdef _cplusplus
}
#endif

#endif