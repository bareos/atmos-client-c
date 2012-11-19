/*
 * atmos_create.c
 *
 *  Created on: Nov 14, 2012
 *      Author: cwikj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atmos.h"
#include "atmos_util.h"

static AtmosCreateObjectRequest*
AtmosCreateObjectRequest_common_init(AtmosCreateObjectRequest *self) {
    ((Object*)self)->class_name = CLASS_ATMOS_CREATE_OBJECT_REQUEST;

    memset(((void*)self)+sizeof(RestRequest), 0,
            sizeof(AtmosCreateObjectRequest) - sizeof(RestRequest));

    return self;
}

AtmosCreateObjectRequest*
AtmosCreateObjectRequest_init(AtmosCreateObjectRequest *self) {
    RestRequest_init((RestRequest*)self, "/rest/objects", HTTP_POST);
    return AtmosCreateObjectRequest_common_init(self);
}

AtmosCreateObjectRequest*
AtmosCreateObjectRequest_init_ns(AtmosCreateObjectRequest *self,
        const char *path) {
    char uri[ATMOS_PATH_MAX];

    if(path[0] != '/') {
        fprintf(stderr, "Path must start with a '/'\n");
        return NULL;
    }

    snprintf(uri, ATMOS_PATH_MAX, "/rest/namespace%s", path);

    RestRequest_init((RestRequest*)self, uri, HTTP_POST);

    return AtmosCreateObjectRequest_common_init(self);
}

void
AtmosCreateObjectRequest_destroy(AtmosCreateObjectRequest *self) {
    memset(((void*)self)+sizeof(RestRequest), 0,
            sizeof(AtmosCreateObjectRequest) - sizeof(RestRequest));

    RestRequest_destroy((RestRequest*)self);
}

void
AtmosCreateObjectRequest_add_metadata(AtmosCreateObjectRequest *self,
        const char *name, const char *value,
        int listable) {
    if(listable) {
        int i = self->listable_meta_count++;
        strcpy(self->listable_meta[i].name, name);
        strcpy(self->listable_meta[i].value, value);
    } else {
        int i = self->meta_count++;
        strcpy(self->meta[i].name, name);
        strcpy(self->meta[i].value, value);
    }
}

void
AtmosCreateObjectRequest_add_acl(AtmosCreateObjectRequest *self,
        const char *principal,
        enum atmos_acl_principal_type type,
        enum atmos_acl_permission permission) {
    int i = self->acl_count++;
    self->acl[i].permission = permission;
    self->acl[i].type = type;
    strcpy(self->acl[i].principal, principal);
}

AtmosCreateObjectResponse*
AtmosCreateObjectResponse_init(AtmosCreateObjectResponse *self) {
    AtmosResponse_init((AtmosResponse*)self);

    memset(((void*)self)+sizeof(AtmosResponse), 0,
            sizeof(AtmosCreateObjectResponse) - sizeof(AtmosResponse));

    ((Object*)self)->class_name = CLASS_ATMOS_CREATE_OBJECT_RESPONSE;

    return self;
}

void
AtmosCreateObjectResponse_destroy(AtmosCreateObjectResponse *self) {
    memset(((void*)self)+sizeof(AtmosResponse), 0,
            sizeof(AtmosCreateObjectResponse) - sizeof(AtmosResponse));

    AtmosResponse_destroy((AtmosResponse*)self);
}

void AtmosFilter_parse_create_response(RestFilter *self, RestClient *rest,
        RestRequest *request, RestResponse *response) {
    const char *location;
    size_t location_len;

    // Do nothing on the request, just pass to the next filter.
    // Pass to the next filter
    if(self->next) {
        ((rest_http_filter)self->next->func)(self->next, rest, request, response);
    }

    // Now we're on the response.

    // Check to make sure we had success before parsing.
    if(response->http_code != 201) {
        return;
    }

    // The ObjectID should be in the Location header
    location = RestResponse_get_header_value(response, HTTP_HEADER_LOCATION);

    if(!location) {
        fprintf(stderr, "Error: Could not find header %s in response",
                HTTP_HEADER_LOCATION);
        return;
    }

    // Header will be /rest/objects/oid
    // Take last 44 digits.
    location_len = strlen(location);
    if(location_len != ATMOS_OID_LENGTH-1+ATMOS_OID_LOCATION_PREFIX_SIZE) {
        fprintf(stderr, "Error: location was %zd bytes; expected %zd",
                location_len,
                (size_t)ATMOS_OID_LENGTH-1+ATMOS_OID_LOCATION_PREFIX_SIZE);
        return;

    }
    strcpy(((AtmosCreateObjectResponse*)response)->object_id,
            location+ATMOS_OID_LOCATION_PREFIX_SIZE);

}

void AtmosFilter_set_create_headers(RestFilter *self, RestClient *rest,
        RestRequest *request, RestResponse *response) {

    AtmosCreateObjectRequest *req = (AtmosCreateObjectRequest*)request;
    AtmosClient *atmos = (AtmosClient*)rest;

    if(req->meta_count > 0) {
        AtmosUtil_set_metadata_header(req->meta, req->meta_count, 0,
                atmos->enable_utf8_metadata, request);
    }

    if(req->listable_meta_count > 0) {
        AtmosUtil_set_metadata_header(req->listable_meta,
                req->listable_meta_count, 1,
                atmos->enable_utf8_metadata, request);
    }

    if(req->acl_count > 0) {
        AtmosUtil_set_acl_header(req->acl, req->acl_count, request);
    }

    // Pass to the next filter
    if(self->next) {
        ((rest_http_filter)self->next->func)(self->next, rest, request, response);
    }

}


