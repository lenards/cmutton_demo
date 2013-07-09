#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>  // for stat() & mkdir()
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>

#include "dbg.h"
#include "libmutton/mutton.h"

#define MAX_PATH_LEN 4096
#define MAX_EVENT_SCRIPTS 120

const static char* DB_PATH = "tmp/demo";
const static mtn_index_partition_t INDEX_PARTITION = 1;
const static char* BUCKET_NAME = "planet-hoth";

const static char* BASIC_EVENT_NAME = "basic";
const static char* BASIC_EVENT_JSON = "{\"a_field\":\"TROLOLOL I'm an Event!!!\"}";

const static char* LUA_SCRIPT_EXT = ".lua";
const static char* LUA_SCRIPT_PATH = "/lua_scripts/";

const static char* LUA_PACKAGE_PATH = "/lua_scripts/packages/?.lua";
const static char* LUA_LIB_PATH = "/lua_scripts/lib/?";

struct stat st = { 0 };


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
                printf("found one: %s\n", scripts[i]);
                i++;
            }
        }
        closedir(dir);
    } else {
        // FAIL!
        return NULL;
    }

    return scripts;
}

int main(int argc, char *argv[])
{
    void *status = NULL;
    void *ctxt = mutton_new_context();
    bool ret = false;
    int rc;
    char *curr_working_dir = NULL;
    char path[MAX_PATH_LEN];
    char script_path[MAX_PATH_LEN];
    char **scripts = NULL;
    char *emessage = NULL;
    char **curr_script = NULL;

    check(ctxt, "Well, that wasn't what we were expecting.");

    if(stat(DB_PATH, &st) == -1) {
        rc = mkdir(DB_PATH, 0770);
        check(rc == 0, "We didn't make the directory after all...");
        printf("what was the return code: %d\n", rc);
    } else {
        printf("so we think that directory, %s, exists... HA!\n", DB_PATH);
    }

    ret = mutton_set_opt(ctxt, MTN_OPT_DB_PATH, (void *)DB_PATH, strlen(DB_PATH), &status);
    check(ret, "Could not set option for the database path...");

    curr_working_dir = getcwd(NULL, MAX_PATH_LEN);
    strlcpy(path, curr_working_dir, MAX_PATH_LEN);
    strlcat(path, LUA_PACKAGE_PATH, MAX_PATH_LEN);
    printf("lua package path: %s\n", path);

    ret = mutton_set_opt(ctxt, MTN_OPT_LUA_PATH, (void *)path,
                        strlen(curr_working_dir), &status);
    check(ret, "Could not set option for lua package path...");

    ret = mutton_set_opt(ctxt, MTN_OPT_LUA_CPATH, (void *)LUA_LIB_PATH,
                        strlen(LUA_LIB_PATH), &status);
    check(ret, "Could not set option for lua library path...");

    ret = mutton_init_context(ctxt, &status);
    check(ret, "Could initialize the context for Mutton...");

    if (ret) {
        printf("well - you don't have to fall on your sword yet...\n");
    }

    strlcpy(script_path, curr_working_dir, MAX_PATH_LEN);
    strlcat(script_path, LUA_SCRIPT_PATH, MAX_PATH_LEN);

    printf("script path: %s\n", script_path);
    printf("extension: %s\n", LUA_SCRIPT_EXT);
    scripts = find_scripts(script_path, LUA_SCRIPT_EXT);
    check(scripts, "Something went horribly wrong with finding the scripts...");

    for(curr_script = scripts; *curr_script; curr_script++) {
        char *basename = NULL;
        char *p = NULL;
        const char *s;

        printf("script: %s\n", *curr_script);
        basename = strrchr(*curr_script, '/') + sizeof(char);
        p = strstr(basename, LUA_SCRIPT_EXT);
        *p = '\0';
        printf("basename: %s\n", basename);
        s = malloc(strlen(curr_script) * sizeof(char));
        strncpy(s, curr_script, strlen(curr_script));

        ret = mutton_register_script_path(ctxt, MTN_SCRIPT_LUA, (void *)basename,
                    strlen(basename), (void *)s, strlen(s),
                    &status);
        check(ret, "Could not register script paths correctly...");
        free(s);
    }

    free(curr_working_dir);
    free(scripts);
    mutton_free_context(ctxt);

    return 0;

error: // if we have an error in check(), we'll jump to here...

    mutton_status_get_message(ctxt, status, &emessage);
    printf("error msg: %s\n", emessage);
    free(emessage);
    // let's clean up before we exit:
    if(curr_working_dir) free(curr_working_dir);
    if(ctxt) mutton_free_context(ctxt);
    if(scripts) free(scripts);
    return -1;
}