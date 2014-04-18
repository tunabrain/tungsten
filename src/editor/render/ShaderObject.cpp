#include <sys/types.h>
#include <sys/stat.h>
#include <GL/glew.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "Debug.hpp"
#include "ShaderObject.hpp"

namespace Tungsten
{

static int fsize(FILE *fp) {
    int prev = ftell(fp);
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, prev, SEEK_SET);
    return size;
}

#ifdef _WIN32
static FILETIME ftime(const char *path) {
    FILETIME result;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    GetFileTime(file, 0, 0, &result);
    CloseHandle(file);
    return result;
}
#else

static time_t ftime(const char *path) {
    struct stat stat;
    int file = open(path, O_RDONLY);
    if (!file)
        return 0;
    fstat(file, &stat);
    close(file);
#if __USE_XOPEN2K8
    return stat.st_mtime;
#else
    return stat.st_mtimespec.tv_sec;
#endif
}
#endif

void ShaderObject::addFile(const char *path)
{
    loadFile(&_sources[_sourceCount++], path);
}

int ShaderObject::refresh()
{
    int compileFlag = 0;

    for (int i = 0; i < _sourceCount; i++) {
#ifdef _WIN32
        FILETIME currTime = ftime(_sources[i].file);
        if (CompareFileTime(&currTime, &_sources[i].timestamp) == 1) {
#else
        if (ftime(_sources[i].file) > _sources[i].timestamp) {
#endif
            loadFile(&_sources[i], _sources[i].file);
            compileFlag = 1;

            printf("Reloading %s\n", _sources[i].file);
            fflush(stdout);
        }
    }

    if (compileFlag)
        compile(_type);

    return compileFlag;
}

void ShaderObject::compile(ShaderType type_)
{
    const GLchar *source[MAX_SOURCES];
    for (int i = 0; i < _sourceCount; i++)
        source[i] = _sources[i].src;

    if (_name)
        glDeleteShader(_name);

    GLuint shader = glCreateShader(type_);
    glShaderSource(shader, _sourceCount, source, NULL);
    glCompileShader(shader);

    _name = shader;
    _type = type_;

    check();
}

void ShaderObject::loadFile(ShaderSource *s, const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        FAIL("Unable to open file '%s'\n", path);

    int size = fsize(file);
    char *src = (char *)malloc((size + 1)*sizeof(char));
    fread(src, sizeof(char), size, file);
    src[size] = '\0';

    s->file = strdup(path);
    s->src = src;
    s->timestamp = ftime(path);
}

void ShaderObject::check()
{
    GLuint obj = _name;
    GLchar tmp[2048];

    int status, length;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    GLchar *log = NULL;
    if (length > 1) {
        log = (GLchar *)malloc(length*sizeof(GLchar));
        glGetShaderInfoLog(obj, length, NULL, log);
    }

    if (!status || length > 1) {
        int src_length;
        glGetShaderiv(obj, GL_SHADER_SOURCE_LENGTH, &src_length);

        GLchar *src = (GLchar *)malloc(src_length*sizeof(GLchar));
        glGetShaderSource(obj, src_length, NULL, src);

        LOG("shader", WARN, "---------------------------\n");
        int line = 1;
        GLchar *src2 = src;
        for (int i = 0; i < src_length; i++, src2++) {
            if (*src2 == '\n') {
                memcpy(tmp, src, src2 - src);
                tmp[src2 - src] = '\0';

                LOG("shader", WARN, "%4d | %s\n", line, tmp);
                src = src2 + 1;
                line++;
            }
        }
        LOG("shader", WARN, "%4d | %s\n", line, src);
        LOG("shader", WARN, "---------------------------\n");
        LOG("shader", WARN, "%s\n", log);
        FAIL("Unable to compile shader\n");
    } else if (length > 1) {
        LOG("shader", WARN, "%s\n", log);
    }
}

}
