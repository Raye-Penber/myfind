/*This is a simplified implementation of the Linux command "find".
Possible parameters are:
-user       finds directory entries of a given user
-name       finds directory entries with a file name matching the supplied pattern
-type       finds directory entries of a given type
-print      prints the name of the directory to stdout
-ls         similiar to -ls command in CLI*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <fnmatch.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>

#define MAXPATHLENGTH 4096

typedef struct stat FileInfo;

typedef struct parameter {
    char* name;
    char* value;
} Parameter;

typedef struct param_node {
    Parameter* param;
    struct param_node* next;
} ParameterNode;

Parameter* createParameter(const char* name, const char* value);
ParameterNode* parseParams(int argc, char* argv[], char* path);
ParameterNode* appendParameter(ParameterNode* head, Parameter* param);
bool typeExists(const char* type);
void verifyArgument(int argc, char* argv[], int index);
void exitOnNull(Parameter* param, const char* paramName);
void* allocateMemory(size_t size);
bool stringStartsWith(const char *pre, const char *str);
bool isNumeric(const char* str);
void doEntry(const char* entry_name, ParameterNode* params);
void doDirectory(const char* dir_name, ParameterNode* params);
char* getFilePermissions(mode_t mode);
void concatPath(char* dest, const char* arg1, const char* arg2);
void printLs(const char* path, FileInfo* fileInfo);
void printPath(const char* path);
bool compUser(const FileInfo* fi, const char* user);
bool compPath(const char* name, const char* path);
bool matchPath(const char* pattern, const char* path);
bool compType(const FileInfo* fileInfo, char type);
bool hasNoUser(const FileInfo* fileInfo);

int main(int argc, char* argv[]) {

    char* path = (char*)allocateMemory(sizeof(char) * MAXPATHLENGTH);
    ParameterNode* params = parseParams(argc, argv, path);

    doEntry(path, params);

    return 0;
}

//Checks argc and argv for used parameters
ParameterNode* parseParams(int argc, char* argv[], char* path) {
    ParameterNode* head = (ParameterNode*)malloc(sizeof(ParameterNode));

    strncpy(path, ".", MAXPATHLENGTH);

    if(argc == 1) {
        appendParameter(head, createParameter("-print", NULL));
        return head;
    }

    bool outputSet = false;

    for (int i = 1; i < argc; i++) {

        if (stringStartsWith("-", argv[i])) {
            if(strcmp("-user", argv[i]) == 0) {
                verifyArgument(argc, argv, i);
                Parameter* userParam = createParameter(argv[i], argv[i+1]);
                exitOnNull(userParam, argv[i]);
                appendParameter(head, userParam);
                i++;
            } else if(strcmp("-name", argv[i]) == 0) {
                verifyArgument(argc, argv, i);

                Parameter* nameParam = createParameter(argv[i], argv[i + 1]);
                exitOnNull(nameParam, argv[i]);
                appendParameter(head, nameParam);
                i++;
            } else if(strcmp("-type", argv[i]) == 0) {
                verifyArgument(argc, argv, i);

                if (!typeExists(argv[i+1])) {
                    fprintf(stderr, "Type does not exist.\n");
                    exit(1);
                }

                Parameter* typeParam = createParameter(argv[i], argv[i+1]);
                exitOnNull(typeParam, argv[i]);
                appendParameter(head, typeParam);
                i++; 
            } else if(strcmp("-ls", argv[i]) == 0) {
                Parameter* lsParam = createParameter(argv[i], NULL);
                exitOnNull(lsParam, argv[i]);
                appendParameter(head, lsParam);
                outputSet = true;
            } else {
                fprintf(stderr, "%s is not a valid command.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else {
            if(i == 1) {
                strncpy(path, argv[i], MAXPATHLENGTH);
            } else {
                fprintf(stderr, "%s is not a valid command.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if(outputSet == false) {
        appendParameter(head, createParameter("-print", NULL));
    }
    return head;
}

//Checks if an argument that needs a variable has one
void verifyArgument(int argc, char* argv[], int index) {
    if((index + 1) >= argc) {
        fprintf(stderr, "No argument provided for %s.\n", argv[index]);
        exit(1);
    }
}

//Checks if a user exists in user database
bool userExists(const char* username, unsigned int* userId) {
    struct passwd* user = getpwnam(username);

    if(user == NULL) {
        return false;
    }
    *userId = user->pw_uid;
    return true;
}

//Checks if a userID exists in user database
bool userIdExists(unsigned int userId) {
    struct passwd* user = getpwuid(userId);

    if(user == NULL) {
        return false;
    }
    return true;
}

//Checks if a group exists in the group database
bool groupExists(const char* groupName) {
    struct group* grp = getgrnam(groupName);

    if(grp == NULL) {
        return false;
    }
    return true;
}

//Checks if a groupID exists in the group database
bool groupIdExists(unsigned int groupId) {
    struct group* grp = getgrgid(groupId);

    if(grp == NULL) {
        return false;
    }
    return true;
}

//Checks if a given type is allowed
bool typeExists(const char* type) {
    static char allowedTypes[7] = {'b', 'c', 'd', 'p', 'f', 'l', 's'};

    for(unsigned int i = 0; i < sizeof(allowedTypes); i++) {
        if(type[0] == allowedTypes[i]) {
            return true;
        }
    }
    return false;
}

/// This function throws an error if a parameter can not be parsed
/// and terminated the program.
/// If a non - parsable parameter is found, the program call is faulty,
/// so the program needs to be terminated. This mirrors the standard
/// behaviour of command line program calls.
void exitOnNull(Parameter* param, const char* paramName) {
    if (param == NULL) {
        fprintf(stderr, "Parameter %s could not be parsed.\n", paramName);
        exit(EXIT_FAILURE);
    }
}

//Appends a paramater to the parameter linked list
ParameterNode* appendParameter(ParameterNode* head, Parameter* param) {
    if(head == NULL || param  == NULL) {
        return NULL;
    }

    if(head->param == NULL) {
        head->param = param;
        head->next = NULL;
        return head;
    }

    ParameterNode* current = head;

    while(current->next != NULL) {
        current = current->next;
    }

    current->next = (ParameterNode*)allocateMemory(sizeof(ParameterNode));
    current->next->param = param;
    current->next->next = NULL;

    return head;
}

/// This function creates and instantiates a new parameter variable with a name and a value.
/// If no name is given, the function returns NULL.
/// If a name is and a value is not given, a new parameter with a name only is created.
/// If name and value is given, a new parameter with a name and a value is created.
Parameter* createParameter(const char* name, const char* value) {
    if(name == NULL) {
        return NULL;
    }

    Parameter* param = (Parameter*)allocateMemory(sizeof(Parameter));

    param->name = (char*)allocateMemory(sizeof(char) * strlen(name + 1));
    strcpy(param->name, name);

    if(value != NULL) {
        param->value = (char*)allocateMemory(sizeof(char) * strlen(value + 1));
        strcpy(param->value, value);
    }

    return param;
}

//Called for every entry to be tested
void doEntry(const char* entry_name, ParameterNode* params) {
    FileInfo* fi = (FileInfo*)allocateMemory(sizeof(FileInfo));

    errno = 0;

    ParameterNode* current = params;
    bool flag = true;

    while((current != NULL) && flag) {
        if(stat(entry_name, fi) != 0) {
            switch (errno) {
                case EACCES:
                    fprintf(stdout, "stat(\"%s\") failed.\n", entry_name);
                    break;
                default:
                    error(EXIT_FAILURE, errno, "stat(\"%s\") failed.\n", entry_name);
                    break;
            }
        }

        if(strcmp(current->param->name, "-print") == 0) {
            printPath(entry_name);
        } else if(strcmp(current->param->name, "-ls") == 0) {
            printLs(entry_name, fi);
        } else if(strcmp(current->param->name, "-user") == 0) {
            flag &= compUser(fi, current->param->value);
        } else if(strcmp(current->param->name, "-type") == 0) {
            flag &= compType(fi, current->param->value[0]);
        } else if(strcmp(current->param->name, "-name") == 0) {
            flag &= compPath(current->param->value, entry_name);
        }

        current = current->next;
    }

    if (S_ISDIR(fi->st_mode)) {
        doDirectory(entry_name, params);
    }

    free(fi);
}

//Called for every directory to be tested
void doDirectory(const char* dir_name, ParameterNode* params){
    errno = 0;
    DIR* dir = opendir(dir_name);

    if(dir == NULL) {
        switch(errno) {
            case EACCES:
                fprintf(stdout, "opendir(%s) failed.\n", dir_name);
                break;
            default: error(EXIT_FAILURE, errno, "opendir(%s) failed.\n", dir_name);
        }
    }

    struct dirent* entry;

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char newPath[MAXPATHLENGTH];
            concatPath(newPath, dir_name, entry->d_name);
            doEntry(newPath, params);
        }
    }
}

//Recreates functionality of "ls" command on CLI
void printLs(const char* path, FileInfo* fileInfo) {
    char timeStrBuff[13];
    time_t time = fileInfo->st_mtim.tv_sec;
    struct tm* lastModtime = localtime(&time);

    strftime(timeStrBuff, 13, "%b %e %H:%M", lastModtime);

    errno = 0;

    struct passwd* user = getpwuid(fileInfo->st_uid);
    struct group* grp = getgrgid(fileInfo->st_gid);

    printf("%10lu", fileInfo->st_ino);
    printf("%7ld", fileInfo->st_blocks / 2);
    printf("%11s", getFilePermissions(fileInfo->st_mode));
    printf("%4lu", fileInfo->st_nlink);

    if(user == NULL) {
        printf("%11u", fileInfo->st_uid);
    } else {
        printf("%11s", user->pw_name);
    }

    if(grp == NULL) {
        printf("%11u", fileInfo->st_gid);
    } else {
        printf("%11s", grp->gr_name);
    }

    printf("%10ld", fileInfo->st_size);
    printf("%13s", timeStrBuff);
    printf(" %s", path);

    printf("\n");
}

//Prints a path
void printPath(const char* path) {
    fprintf(stdout, "%s\n", path);
}

//Returns file permissions through mode_t flags
char* getFilePermissions(mode_t mode) {
    char* bits = malloc(sizeof(char) * 11);

    bits[0] = S_ISDIR(mode) ? 'd' : '-';
    bits[1] = mode & S_IRUSR ? 'r' : '-';
    bits[2] = mode & S_IWUSR ? 'w' : '-';
    bits[3] = mode & S_IXUSR ? 'x' : '-';
    bits[4] = mode & S_IRGRP ? 'r' : '-';
    bits[5] = mode & S_IWGRP ? 'w' : '-';
    bits[6] = mode & S_IXGRP ? 'x' : '-';
    bits[7] = mode & S_IROTH ? 'r' : '-';
    bits[8] = mode & S_IWOTH ? 'w' : '-';
    bits[9] = mode & S_IXOTH ? 'x' : '-';
    bits[10] = '\0';

    return bits;
}

//Concatenates two strings and adds a '/' between them
void concatPath(char* dest, const char* arg1, const char* arg2) {
    int pathLength = snprintf(dest, MAXPATHLENGTH, "%s/%s", arg1, arg2);

    if(pathLength >= MAXPATHLENGTH) {
        error(EXIT_FAILURE, errno, "Maximum path length exceeded.\n");
    }
}

//Checks if a user matches with the user of the provided file
bool compUser(const FileInfo* fi, const char* user) {
    long userId = -1;

    if(isNumeric(user) == true && strlen(user) < 19) {
        if(strtol(user, NULL, 10) == 0) {
            error(EXIT_FAILURE, 1, "Failed converting user ID.\n");
        }

        return fi->st_uid == userId;
    }

    errno = 0;

    struct passwd* pwd_user = getpwnam(user);

    if (pwd_user == NULL) {
        error(EXIT_FAILURE, errno, "User does not exist.\n");
    }

    return fi->st_uid == pwd_user->pw_uid;
}

//Matches a file name against a pattern
bool compPath(const char* name, const char* path) {
    char* buff = strdup(path);
    char* extractedFileName = basename(buff);

    return fnmatch(name, extractedFileName, FNM_NOESCAPE ) != FNM_NOMATCH;
}

//Matches a path against a pattern
bool matchPath(const char* pattern, const char* path) {
    return fnmatch(pattern, path, FNM_NOESCAPE) != FNM_NOMATCH;
}

//Checks type of a file
//S_ISREG() - regular file
//S_ISDIR() - directory
//S_ISCHR() - character file
//S_ISBLK() - block file
//S_ISFIFO() - FIFO
//S_ISLINK() - symbolic link
//S_ISSOCK() - socket
bool compType(const FileInfo* fileInfo, char type) {
    mode_t fileType = fileInfo->st_mode;

    switch (type) {
        case 'b':
            return S_ISBLK(fileType);
        case 'c':
            return S_ISCHR(fileType);
        case 'd':
            return S_ISDIR(fileType);
        case 'p':
            return S_ISFIFO(fileType);
        case 'f':
            return S_ISREG(fileType);
        case 'l':
            return S_ISLNK(fileType);
        case 's':
            return S_ISSOCK(fileType);
        default:
            return false;
    }
}

//Checks if a file has a user
bool hasNoUser(const FileInfo* fileInfo) {
    return getpwuid(fileInfo->st_uid) == NULL;
}

//Checks if malloc was successful
void* allocateMemory(size_t size) {
    void* ptr = malloc(size);

    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

//Checks if a string starts with a given string; eg.: pre = '-', str = '-type' -> TRUE
bool stringStartsWith(const char *pre, const char *str) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

//Checks if a string contains only numbers
bool isNumeric(const char* str) {
    if(str == NULL) return false;

    for(unsigned int i = 0; i < strlen(str); i++) {
        if(str[i] < '0' || str[i] > '9') {
            return false; 
        }
    }
    return true;
}