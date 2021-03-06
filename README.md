This API allows C developers to easily connect to an Atmos based Storage cloud.
It handles all of the low-level tasks such as generating and signing 
requests, connecting to the server, and parsing server responses. 

---
# Requirements
* libcurl 7.20 or greater
* openssl-dev .9.8k or greater
* libxml2 2.4 or greater (xsltproc must be on your path)

__Ubuntu:__

        sudo apt-get install libcurl4-openssl-dev libssl-dev libxml2 libxml2-dev xsltproc
        
__CentOS/RHEL:__

        sudo yum install openssl.x86_64 openssl-devel.x86_64 libxml2 libxml2-devel.x86_64 libcurl-devel.x86_64 libxslt.x86_64 libcurl.x86_64 libcurl-devel.x86_64

---
# Building 
This package uses autotools, so the standard GNU build procedure can be used:

    ./configure
    make
    sudo make install


_Note that the github repo does not include any generated files from autotools._
_You need to run autogen.sh first.  This requires the packages libtool, automake, and aclocal._  
_If you don't want to bother with that you should download a tarball release instead._

---
# Documentation
The documentation is embedded in the source using Doxygen format.  To generate
the documentation, simply install doxygen and run: `doxygen Doxyfile`.  

The generated documentation can be accessed by opening html/index.html.

---
# Running the unit tests
To run the unit tests, copy the *atmostest.ini.template* file to *atmostest.ini* and
replace the values with ones suitable for your environment.  You will need at
least two UIDs in the same subtenant.  The proxy support lines are optional and
can be omitted if you don't have a proxy.

Once the *tests/atmostest.ini* file is configured, you can run the unit tests with

        make check



To run the unit tests under GDB, do the following

        make
        cd tests
        libtool --mode execute gdb check_atmos

On OSX, you need to use *../libtool* to use the GNU version of libtool instead
of OSX's libtool.

---
# Getting Started


### Objects

This SDK uses an object-oriented pattern implemented in C.  For more
information, see *dep/rest-client-c/include/maindoc.h* or use *doxygen* to 
build the documentation from the rest-client-c subproject.

At a high level, you must `_init` and `_destroy` all of your objects.  Also, since
the first member of the object structures is always the parent structure, it
is valid to cast a structure pointer to any of its superclasses, e.g.

        AtmosCreateObjectRequest *create_request;
        RestRequest *rest_request = (RestRequest*)create_request;
is valid.  You can then call methods on the parent class:

        RestRequest_set_file_body(rest_request, f, 1024, "application/octet-stream");



### AtmosClient

The first operation will be to initialize an AtmosClient object.  You can do 
this either with a stack-based object:

        AtmosClient atmos;
        AtmosClient_init(&atmos, "http://api.atmosonline.com", -1, "<uid>", "<secret>");   
        // <<do your AtmosClient_xxxx() functions.>>
        // ...
        AtmosClient_destroy(&atmos);
    
    
or a dynamic object:

        AtmosClient *atmos = AtmosClient_init(malloc(sizeof(AtmosClient)));
        // do your AtmosClient_xxxx() functions.
        AtmosClient_destroy(atmos);
        free(atmos);


### AtmosClient_xxxx functions

The functions on AtmosClient generally come in two flavors: *simple* and
*full*.  The *simple* versions take common arguments as parameters and execute
the request.  The *full* versions take a `Atmos<Operation>Request` object and
allow you to fully configure all of the operations on the request.  For example,
if you simply want to read an object, you use `AtmosClient_read_object_simple()`.
If you want to read an object range (less common), you construct an 
AtmosReadObjectRequest object, configure the range, and pass the request object
to `AtmosClient_read_object()`.

Simple:

    AtmosReadObjectResponse response;
    // Init the response
    AtmosReadObjectResponse_init(&response);
    // Execute the request
    AtmosClient_read_object_simple(&atmos, object_id, &response);
    // <<Handle the response object.>>
    // ...
    // Cleanup
    AtmosReadObjectResponse_destroy(&response);

Full:

    AtmosReadObjectRequest request;
    AtmosReadObjectResponse response;

    // Init and configure the request
    AtmosReadObjectRequest_init(&request, object_id);
    AtmosReadObjectRequest_set_range(&request, 0, 5);

    // Init the response
    AtmosReadObjectResponse_init(&response);

    // Execute the request
    AtmosClient_read_object(&atmos, &request, &response);

    // <<Handle the response object.>>
    // ...

    // Cleanup
    AtmosReadObjectResponse_destroy(&response);
    AtmosReadObjectRequest_destroy(&request);

### Error Handling

To handle errors, you first should check the RestRequest parent's http_code.
Successful operations will return in the 2XX range, depending on the request. If
a transport-level error occurs, the http_code will be zero.  In this case, you
can get the lower-level error from the curl_error member.  For nonzero http_code
errors (e.g. 4XX and 5XX errors), you can usually get the Atmos error code and
message from the AtmosResponse parent object.

Sample error handler:


    int check_error(AtmosResponse *response) {
        RestResponse *rest_response = (RestResponse*) response;
        if (rest_response->http_code == 0) {
            // Transport-level error
            printf("Connection error %d: %s\n", rest_response->curl_error,
                    rest_response->curl_error_message);
            return 1;
        } else if (rest_response->http_code > 399) {
            printf("HTTP error %d: %s\n", rest_response->http_code,
                    rest_response->http_status);
            return 1;
        }
        if (response->atmos_error > 0) {
            printf("Atmos error %d: %s\n", response->atmos_error,
                    response->atmos_error_message);
            return 1;
        }
        return 0;
    }

