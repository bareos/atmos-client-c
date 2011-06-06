#ifndef __AOL_INIT__
#define __AOL_INIT__

#include "transport.h"


typedef struct requestval {
    credentials c;
    char ** emc_headers;
    char * path;
    char * method;
    char * range;
    char * content_type;
    char * date;
    char * uid;
    char * signature;
} request;

typedef struct ACLval {
    char user[1024];
    char permission[1024];
    void *next;
} acl;


typedef struct sys_info {
    char version[1024];
} sys_info;

#define TIMESIZE 40
#define UIDSIZE  44
#define POLICYSIZE  44
#define GIDSIZE  44
#define OBJECTIDSIZE  45

typedef struct System_meta {
    char atime[TIMESIZE];
    char mtime[TIMESIZE];
    char ctime[TIMESIZE];
    char itime[TIMESIZE];
    char type[1024];
    char uid[UIDSIZE];
    char gid[GIDSIZE];
    char objectid[OBJECTIDSIZE];
    char objname[1024];
    unsigned long long size;
    int nlink;
    char policyname[POLICYSIZE];
} system_meta;


//metavalues are max size 1k
typedef struct Metaval {
    char key[1024];
    char value[1024];
    int listable;
    void *next;
} user_meta;

typedef struct listing {
    char name[256];
    char type[128];
} listing;

static const char *EMC_META_HDR_STR = "x-emc-meta: ";
static const char *EMC_USER_HDR_STR = "x-emc-user: ";
//static const char *EMC_GROUPACL_HDR_STR = "x-emc-groupacl";
//static const char *EMC_USERACL_HDR_STR = "x-emc-useracl";
static const char *EMC_LISTABLE_META_HDR_STR = "x-emc-listable-meta: ";

static const char* atime="atime";
static const char* mtime="mtime";
static const char* itime="itime";
static const char* emc_ctime="ctime";
static const char* type="type";
static const char* uid="uid";
static const char* gid="gid";
static const char* objectid="objectid";
static const char* objname="objname";
static const char* size="size";
static const char* nlink="nlink";
static const char* policyname="policyname";

//Object - CRUD
int create_obj(credentials *c, char *object_id, ws_result* ws);
int read_obj(credentials *c, char *object_id, postdata* d, int limit, ws_result* ws);
int update_obj(credentials *c, char *object_id, char* content_type, acl* acl, postdata* data, user_meta* meta, ws_result *ws);
int delete_obj(credentials *c, char *object_od, ws_result *ws);

//Namespace
void rename_ns(credentials *c, char * uri, char *new_uri, int force, ws_result *ws);
void create_ns(credentials *c, char * uri, char *content_type, acl *acl, user_meta *meta, ws_result *ws);
void list_ns(credentials *c, char * uri, postdata*d, int count, ws_result *ws);
void update_ns(credentials *c, char * uri, char *content_type, acl *acl, postdata* data, user_meta *meta, ws_result *ws);
int delete_ns(credentials *c, char *object_id, ws_result *ws);
int set_meta_ns(credentials *c, const char *object_name, const char *key, const char *val);
int get_meta_ns(credentials *c,const char *object_name);

//namespace metadata
int user_meta_ns(credentials *c, const char *uri, char * content_type, user_meta *meta, ws_result* ws);

int set_acl_obj(credentials *c, char *objectid, acl* acl, ws_result *ws);
int get_acl_obj(credentials *c, char *objectid, acl* acl, ws_result *ws);
int get_info_obj(credentials *c, char *objectid, ws_result* ws);//returns object details - replica's etc
int list_obj(credentials *c, char* objectid,char *tag, ws_result * ws); //retrieves all obj id's indexed by tag

int get_sysmd_obj(credentials *c,const char *object_name, ws_result *ws);
int get_usermd_obj();

int set_meta_ns(credentials *c, const char *object_name, const char *key, const char *val);
int get_meta_ns(credentials *c,const char *object_name);


//versionining - new
int create_version();
int delete_version();
int list_versions();
int restore_version();


//Take a ws_result and break its headers into system and user meta structs
void parse_headers(ws_result*, system_meta*, user_meta**);

//atmos specific helpers
credentials* init_ws(const char *user_id, const char *key, const char *endpoint);

//user meta data helpers
user_meta* new_user_meta(char *key, char *value, int listable);
void add_user_meta(user_meta *head, char *key, char *value, int listable);
void free_user_meta(user_meta *um);


void get_service_info(credentials *c, ws_result *result) ;



#endif
