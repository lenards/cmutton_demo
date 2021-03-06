#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>   // for stat() & mkdir()
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>     // for basename()

#include "dbg.h"
#include "libmutton/mutton.h"

#define MAX_PATH_LEN 4096
#define MAX_EVENT_SCRIPTS 120

const static char* DB_PATH = "tmp/demo";
const static mtn_index_partition_t INDEX_PARTITION = 1;
const static char* BUCKET_NAME = "planet-hoth";

const static char* BASIC_EVENT_NAME = "basic";
const static char* BASIC_EVENT_JSON = "{\"a_field\":\"TROLOLOL I'm an Event!!!\"}";
const static char* BASIC_EVENT_JSON1 = "{\"a_field\":\"WHAT WHAT I'm a signup event!!!\"}";
const static char* BASIC_EVENT_JSON2 = "{\"a_field\":\"Boo! I'm a cancellation event!!!\"}";
const static char* BASIC_EVENT_JSON3 = "{\"snarf_3\":\"sign-up; campaign=20130701\"}";

const static char* LUA_SCRIPT_EXT = ".lua";
const static char* LUA_SCRIPT_PATH = "/lua_scripts/";

const static char* LUA_PACKAGE_PATH = "/lua_scripts/packages/?.lua";
const static char* LUA_LIB_PATH = "/lua_scripts/lib/?";


char** find_scripts(char *path, const char *extension) {
    char **scripts;
    int i = 0;
    DIR *dir;
    struct dirent *ent;
    int malsize = -1;
    int path_len = strlen(path);

    scripts = malloc(MAX_EVENT_SCRIPTS * sizeof(char *));
    scripts[MAX_EVENT_SCRIPTS-1] = NULL;

    dir = opendir(path);
    if(dir != NULL) {
        while((ent = readdir(dir)) != NULL) {
            if(strstr(ent->d_name, extension)) {
                malsize = (path_len + strlen(ent->d_name) + 1) * sizeof(char);
                scripts[i] = malloc(malsize);

                strlcpy(scripts[i], path, malsize);
                strlcat(scripts[i], ent->d_name, malsize);
                i++;
            }
        }
        closedir(dir);
    } else {
        // FAIL!
        return NULL;
    }

    scripts[i] = NULL;

    return scripts;
}

void *initialize_mutton()
{
    struct stat st = { 0 };
    void *status = NULL;
    bool ret = false;
    int rc;
    char *curr_working_dir = NULL;
    char package_path[MAX_PATH_LEN];
    char library_path[MAX_PATH_LEN];
    char script_path[MAX_PATH_LEN];
    char **scripts = NULL;
    char *emessage = NULL;
    void *ctxt = mutton_new_context();

    check(ctxt, "Well, that wasn't what we were expecting.");

    curr_working_dir = getcwd(NULL, MAX_PATH_LEN);
    strlcpy(package_path, curr_working_dir, MAX_PATH_LEN);
    strlcat(package_path, LUA_PACKAGE_PATH, MAX_PATH_LEN);

    strlcpy(library_path, curr_working_dir, MAX_PATH_LEN);
    strlcat(library_path, LUA_LIB_PATH, MAX_PATH_LEN);

    strlcpy(script_path, curr_working_dir, MAX_PATH_LEN);
    strlcat(script_path, LUA_SCRIPT_PATH, MAX_PATH_LEN);

    log_err("\n\npackage-path: %s \n", package_path);
    log_err("\n\nlibrary-path: %s \n", library_path);
    log_err("\n\n script-path: %s \n", script_path);

    if(stat(DB_PATH, &st) == -1) {
        rc = mkdir(DB_PATH, 0770);
        check(rc == 0, "We didn't make the directory after all...");
    }

    ret = mutton_set_opt(ctxt, MTN_OPT_DB_PATH, (void *)DB_PATH, strlen(DB_PATH), &status);
    check(ret, "Could not set option for the database path...");

    ret = mutton_set_opt(ctxt, MTN_OPT_LUA_PATH, (void *)package_path,
                        strlen(package_path), &status);
    check(ret, "Could not set option for lua package path...");


    ret = mutton_set_opt(ctxt, MTN_OPT_LUA_CPATH, (void *)library_path,
                        strlen(library_path), &status);
    check(ret, "Could not set option for lua library path...");

    ret = mutton_init_context(ctxt, &status);
    check(ret, "Could initialize the context for Mutton...");

    if (ret) {
        printf("well - you don't have to fall on your sword yet...\n");
    }

    scripts = find_scripts(script_path, LUA_SCRIPT_EXT);
    check(scripts, "Something went horribly wrong with finding the scripts...");

    for(int i = 0; scripts[i] != NULL; i++) {
        char *base = NULL;
        char *p = NULL;

        base = basename(scripts[i]);
        p = strrchr(base, '.');
        *p ='\0';

        ret = mutton_register_script_path(ctxt, MTN_SCRIPT_LUA, (void *)base,
                    strlen(base), (void *)(scripts[i]), strlen(scripts[i]),
                    &status);
        check(ret, "Could not register script paths correctly...");
    }

    free(curr_working_dir);
    free(scripts);

    return ctxt;

error:
    mutton_status_get_message(ctxt, status, &emessage);
    printf("error msg: %s\n", emessage);
    free(emessage);

    // let's clean up before we exit:
    if(curr_working_dir) free(curr_working_dir);
    if(ctxt) mutton_free_context(ctxt);
    if(status) mutton_free_status(status);
    if(scripts) free(scripts);
    return NULL;
}


int main(int argc, char *argv[])
{
    void *status = NULL;
    char *emessage = NULL;
    bool ret = false;
    void *ctxt = initialize_mutton();

    check(ctxt, "Well, that wasn't what we were expecting.");

    ret = mutton_process_event_bucketed(ctxt, INDEX_PARTITION,
                    (void *)BUCKET_NAME, strlen(BUCKET_NAME),
                    (void *)BASIC_EVENT_NAME, strlen(BASIC_EVENT_NAME),
                    (void *)BASIC_EVENT_JSON, strlen(BASIC_EVENT_JSON),
                    &status);
    check(ret, "Could not process the basic event 1... ");

    if (status != NULL) {
        mutton_status_get_message(ctxt, status, &emessage);
        log_err("status: %s\n", emessage);
    }


    ret = mutton_process_event_bucketed(ctxt, INDEX_PARTITION,
                    (void *)BUCKET_NAME, strlen(BUCKET_NAME),
                    (void *)BASIC_EVENT_NAME, strlen(BASIC_EVENT_NAME),
                    (void *)BASIC_EVENT_JSON1, strlen(BASIC_EVENT_JSON1),
                    &status);
    check(ret, "Could not process the basic event 2... ");

    ret = mutton_process_event_bucketed(ctxt, INDEX_PARTITION,
                    (void *)BUCKET_NAME, strlen(BUCKET_NAME),
                    (void *)BASIC_EVENT_NAME, strlen(BASIC_EVENT_NAME),
                    (void *)BASIC_EVENT_JSON2, strlen(BASIC_EVENT_JSON2),
                    &status);
    check(ret, "Could not process the basic event 3... ");

    ret = mutton_process_event_bucketed(ctxt, INDEX_PARTITION,
                    (void *)BUCKET_NAME, strlen(BUCKET_NAME),
                    (void *)BASIC_EVENT_NAME, strlen(BASIC_EVENT_NAME),
                    (void *)BASIC_EVENT_JSON3, strlen(BASIC_EVENT_JSON3),
                    &status);
    check(!ret, "Wait - what happened?!?!");
    log_err("I'm confused... %s\n", (ret ? "true" : "false"));

    mutton_free_context(ctxt);

    log_err("Are we segfaulting when we free the context???\n");

    return 0;

error: // if we have an error in check(), we'll jump to here...
    mutton_status_get_message(ctxt, status, &emessage);
    printf("error msg: %s\n", emessage);
    free(emessage);
    // let's clean up before we exit:
    if(ctxt) mutton_free_context(ctxt);
    if(status) mutton_free_status(status);
    return -1;
}
