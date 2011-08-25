#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "curl/curl.h"
#include "transport.h"
#include "atmos_util.h"
#include "crypto.h"

#ifdef WIN32
/* Windows does not have gettimeofday */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
 
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag;
 
  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);
 
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tmpres /= 10;  /*convert into microseconds*/
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
 
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
 
  return 0;
}

#endif

#define HEADER_MAX 1024

static const char *namespace_uri = "/rest/namespace";
//static const char *object_uri = "/rest/objects";
size_t readfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
     if(stream) {
	postdata *ud = (postdata*)stream;
	if(ud->bytes_remaining > 0) {
	    if(ud->bytes_remaining >= size*nmemb) {
	      if(ud->bytes_written == 0) {
	        memcpy(ptr, ud->data, size*nmemb);
	      } else {
	        memcpy(ptr, ud->data+ud->bytes_written, size*nmemb);
	      }
		ud->bytes_written+=size*nmemb;
		ud->bytes_remaining -=size*nmemb;
		return size*nmemb;
	    } else {
	      unsigned int datasize = (unsigned int)ud->bytes_remaining;
	      memcpy(ptr, ud->data+ud->bytes_written, datasize);
	      ud->bytes_written+=datasize;
	      ud->bytes_remaining=0;
	      return datasize;
	    }
	}
    }
    return 0;
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ws_result *ws = (ws_result*)stream;
    void *new_response = NULL;
    unsigned long long data_offset = ws->body_size;
    size_t mem_required = size*nmemb;
    ws->body_size+= mem_required;
    /* Add an extra byte so we can null terminate it */
    new_response = realloc(ws->response_body,ws->body_size+1);
    if(new_response) {
      ws->response_body = (char*)new_response;
    } else {
      ;
    }
    
    if(data_offset) {
      memcpy(ws->response_body+data_offset,ptr, mem_required);
    } else {
      memcpy(ws->response_body,ptr, mem_required);
    }

    /* Null terminate the body in case it's a string*/
    /* Since the data is one longer there is room, but */
    /* shouldn't affect non-string data as long as the */
    /* body_size element is used when copying*/
    ws->response_body[ws->body_size] = 0;
    return mem_required;
}

size_t headerfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ws_result *ws = (ws_result*)stream;
    size_t mem_required = size*nmemb-2; // - 2 for the /r/n at the end of each header

    ws->headers[ws->header_count] = (char*)malloc(mem_required+1);//+1 for \0
    memcpy(ws->headers[ws->header_count],ptr, mem_required);
    ws->headers[ws->header_count][mem_required] = '\0';
    ws->header_count++;
    //                            
    return size*nmemb;
}

void result_init(ws_result *result) {
    result->return_code = -1;
    result->response_body = NULL;
    result->body_size = 0;
    memset(result->headers, 0,sizeof(result->headers));
    result->header_count=0;

    result->duration_ms = 0;
}

void result_deinit(ws_result* result) {
	int i = 0;
	free(result->response_body);
	for(; i < result->header_count; i++) {
		free(result->headers[i]);
	}

	if(result->content_type != NULL) {
		free(result->content_type);
		result->content_type = NULL;
	}

}

const char *http_request_ns(credentials *c, http_method method, const char *uri,char *content_type, char **headers, int header_count, postdata * data, ws_result* ws_result) {
	
    char * ns_uri = NULL;
    ns_uri = (char*)malloc(strlen(uri)+strlen(namespace_uri)+1);    
    sprintf(ns_uri,"%s%s",namespace_uri, uri);
    http_request(c, method, ns_uri, content_type, headers, header_count, data, ws_result);    
    free((char*)ns_uri);
    return NULL;
}

const char *http_request(credentials *c, http_method method, const char *uri,
		char *content_type, char **headers, int header_count,
		postdata *data, ws_result* ws_result)
{

  //    if(!curl_code) {
  //;
  //} else {

    CURL  *curl = curl_easy_init();
    CURLcode result_code;
    const int connect_timeout = 200;
    char date[256];
    char uidheader[HEADER_MAX];
    char dateheader[HEADER_MAX];
    char *endpoint_url = NULL;
    size_t endpoint_size;
    char errorbuffer[HEADER_MAX*HEADER_MAX];
    struct curl_slist *chunk = NULL;
    char hash_string[HEADER_MAX];
    char range[HEADER_MAX];
    char signature[HEADER_MAX];
    char content_type_header[HEADER_MAX];
    int i;
    char *signed_hash = NULL;
    char content_length_header[HEADER_MAX];
    char range_header[HEADER_MAX];
    struct timeval start_time;
    struct timeval end_time;
    curl_global_init(CURL_GLOBAL_ALL);

    result_init(ws_result); //MUST DEALLOC

    memset(range, 0, HEADER_MAX);
    get_date(date);
    snprintf(dateheader,HEADER_MAX,"X-Emc-Date:%s", date);
    snprintf(uidheader,HEADER_MAX,"X-Emc-Uid:%s",c->tokenid);    
    headers[header_count++] = dateheader;
    //FIXME groupacl headers breaks sig string
    headers[header_count++] = uidheader;

    if (!content_type) {
	content_type = "application/octet-stream";
    }
    
    
    endpoint_size = strlen(c->accesspoint)+strlen(uri) +1;
    endpoint_url = (char*)malloc(endpoint_size);
    
    snprintf(endpoint_url, endpoint_size, "%s%s", c->accesspoint, uri);

    // set up flags this should move into transport layercyrk
    if (curl) {
	//curl_version_info_data *version_data = curl_version_info(CURLVERSION_NOW);

	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_URL, endpoint_url);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 0);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuffer);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, false);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writefunc);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &headerfunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ws_result);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, ws_result);
	
	switch(method) {

	case POST:
	    curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1l);	
	    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, 0L);
	    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);

	    break;
	case PUT:
	    curl_easy_setopt(curl, CURLOPT_PUT, 1L); 
	    break;
	case aDELETE:
	    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); 
	    break;
	case HEAD:
	  curl_easy_setopt(curl, CURLOPT_NOBODY, 1l);
	    break;
	case GET:

	  break;
	case OPTIONS:
	  break;
	}

	if(data) {
	  if(data->offset >= 0) {
	    snprintf(range, HEADER_MAX, "Bytes=%jd-%jd", (intmax_t)data->offset,
	    		(intmax_t)(data->offset+data->body_size-1));
	  }
	  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
			  (curl_off_t)data->body_size);

	  /* If a stream handle is used, let libcurl handle the file I/O */
	  if(data->data_stream) {
		  curl_easy_setopt(curl, CURLOPT_READDATA, data->data_stream);
		  curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
	  } else {
		  curl_easy_setopt(curl, CURLOPT_READDATA, data);
		  curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);
	  }
	}
 
	build_hash_string(hash_string, method, content_type, range,NULL,uri, headers,header_count);
	if(data){
	  data->bytes_written=0;
	  data->bytes_remaining=data->body_size;
	  if(method != GET) {
	    snprintf(content_length_header,HEADER_MAX, "content-length: %jd",
	    		(intmax_t)data->body_size);
	    headers[header_count++]=content_length_header;
	  }
	  
	  if(data->offset >= 0 ) {
	      memset(range_header, 0, HEADER_MAX);
	      snprintf(range_header, HEADER_MAX, "range: Bytes=%jd-%jd",
	    		  (intmax_t)data->offset,
	    		  (intmax_t)(data->offset+data->body_size-1));
	      headers[header_count++] = range_header;
	  }
	}

	for(i=0;i<header_count; i++) {
	    if(headers[i] != NULL) { 
		chunk = curl_slist_append(chunk, headers[i]);	
	    } else {
		; //a null header thats been added to the array is proabbly a bug.
	    }
	}
	

	signed_hash = (char*)sign(hash_string,c->secret);
	snprintf(signature,HEADER_MAX,"X-Emc-Signature:%s", signed_hash);
	free(signed_hash);
	snprintf(content_type_header, HEADER_MAX,"content-type:%s", content_type); 
	curl_slist_append(chunk,"Expect:");
	curl_slist_append(chunk,"Transfer-Encoding:");
	curl_slist_append(chunk, content_type_header);
	curl_slist_append(chunk,signature);
	result_code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	gettimeofday(&start_time, NULL);
	result_code = curl_easy_perform(curl);
	gettimeofday(&end_time, NULL);	
	
	ws_result->duration_ms = end_time.tv_usec - start_time.tv_usec ;	
	ws_result->duration_sec = end_time.tv_sec - start_time.tv_sec ;	
	
	if(ws_result->duration_ms  < 0 ) {
	  ws_result->duration_sec -=1;
	  ws_result->duration_ms +=1000000;
	}
	
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ws_result->return_code);
	curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ws_result->content_type);
	/* dup it so we can free later */
	if(ws_result->content_type) {
	    ws_result->content_type = strdup(ws_result->content_type);
	} else {
	    ws_result->content_type = NULL;
	}

	curl_easy_cleanup(curl);
	curl_slist_free_all(chunk);
    }

    free(endpoint_url);
    return  NULL;
}


void print_ws_result(ws_result *result) {
    char *body = malloc(result->body_size+1);
    memcpy(body, result->response_body, result->body_size);
    body[result->body_size] = '\0';
}
