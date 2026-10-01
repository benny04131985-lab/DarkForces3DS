/* DF3DS V36 - basename/dirname owner for newlib 3DS. */
#include <stddef.h>
#include <string.h>

static char g_dot[] = ".";
static char g_root[] = "/";

char* basename(char* path)
{
    if (!path || !*path)
        return g_dot;

    size_t len = strlen(path);

    while (len > 1 && path[len - 1] == '/')
    {
        path[len - 1] = '\0';
        --len;
    }

    if (len == 1 && path[0] == '/')
        return g_root;

    char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

char* dirname(char* path)
{
    if (!path || !*path)
        return g_dot;

    size_t len = strlen(path);

    while (len > 1 && path[len - 1] == '/')
    {
        path[len - 1] = '\0';
        --len;
    }

    char* slash = strrchr(path, '/');

    if (!slash)
        return g_dot;

    while (slash > path && slash[-1] == '/')
        --slash;

    if (slash == path)
    {
        path[1] = '\0';
        return path;
    }

    *slash = '\0';
    return path;
}
