#include "Version.hpp"
#include "../tungsten/Shared.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <lodepng/lodepng.h>
#include <civetweb/civetweb.h>
#include <sstream>

using namespace Tungsten;

static const int OPT_PORT    = 100;
static const int OPT_LOGFILE = 101;

static struct mg_context *context = nullptr;
static StandaloneRenderer *renderer = nullptr;

static std::mutex logMutex;
static std::ostringstream logStream;

enum MimeType
{
    MIME_TEXT,
    MIME_IMAGE,
    MIME_JSON,
};

const char *mimeTypeToString(MimeType type)
{
    switch (type) {
    case MIME_IMAGE:
        return "image/png";
    case MIME_JSON:
        return "application/json; charset=utf-8";
    case MIME_TEXT:
    default:
        return "text/plain; charset=utf-8";
    }
}

void serveData(struct mg_connection *conn, const void *src, size_t length, MimeType type)
{
    std::string header = tfm::format(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %i\r\n"
        "\r\n",
        mimeTypeToString(type),
        length);

    mg_write(conn, reinterpret_cast<const void *>(header.c_str()), header.size());
    mg_write(conn, src, length);
}

void closeConnection()
{
    if (context) {
        mg_stop(context);
        context = nullptr;
    }
}

int serveLogFile(struct mg_connection *conn, void * /*cbdata*/)
{
    std::string log;
    if (renderer) {
        std::unique_lock<std::mutex> lock(renderer->logMutex());
        log = logStream.str();
    }
    serveData(conn, reinterpret_cast<const void *>(log.c_str()), log.size(), MIME_TEXT);

    return 1;
}

int serveStatusJson(struct mg_connection *conn, void * /*cbdata*/)
{
    std::string statusString;
    if (renderer) {
        RendererStatus status = renderer->status();
        rapidjson::Document document;

        *(static_cast<rapidjson::Value *>(&document)) = status.toJson(document.GetAllocator());

        rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
        rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> jsonWriter(buffer);
        document.Accept(jsonWriter);

        statusString = buffer.GetString();
    }
    serveData(conn, reinterpret_cast<const void *>(statusString.c_str()), statusString.size(), MIME_JSON);

    return 1;
}

int serveFrameBuffer(struct mg_connection *conn, void * /*cbdata*/)
{
    if (!renderer)
        return 0;

    Vec2i res;
    std::unique_ptr<Vec3c[]> ldr = renderer->frameBuffer(res);
    if (!ldr)
        return 0;

    Tungsten::uint8 *encoded = nullptr;
    size_t encodedSize;
    if (lodepng_encode_memory(&encoded, &encodedSize, &ldr[0].x(), res.x(), res.y(), LCT_RGB, 8) != 0)
        return 0;
    ldr.reset();

    std::unique_ptr<Tungsten::uint8, void (*)(void *)> data(encoded, free);
    serveData(conn, reinterpret_cast<const void *>(data.get()), encodedSize, MIME_IMAGE);

    return 1;
}

int main(int argc, const char *argv[])
{
    CliParser parser("tungsten_server", "[options] scene1 [scene2 [scene3...]]");

    parser.addOption('p', "port", "Port to listen on. Defaults to 8080", true, OPT_PORT);
    parser.addOption('l', "log-file", "Specifies a file to save the render log to", true, OPT_LOGFILE);

    renderer = new StandaloneRenderer(parser, logStream);

    parser.parse(argc, argv);

    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "tungsten_server, version " << VERSION_STRING << std::endl;
        std::exit(0);
    }

    Path logFile;
    if (parser.isPresent(OPT_LOGFILE))
        logFile = Path(parser.param(OPT_LOGFILE)).absolute();

    renderer->setup();

    std::string port = "8080";
    if (parser.isPresent(OPT_PORT))
        port = parser.param(OPT_PORT);

    const char *options[] = {
        "listening_ports", port.c_str(),
        "enable_directory_listing", "no",
        "hide_files_patterns", "**",
        "num_threads", "2", // Let's not get too crazy with threads - we don't expect many concurrent requests
        nullptr
    };

    std::atexit(&closeConnection);

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    context = mg_start(&callbacks, 0, options);

    mg_set_request_handler(context, "/log", &serveLogFile, nullptr);
    mg_set_request_handler(context, "/status", &serveStatusJson, nullptr);
    mg_set_request_handler(context, "/render", &serveFrameBuffer, nullptr);

    while (renderer->renderScene());

    if (!logFile.empty()) {
        OutputStreamHandle out = FileUtils::openOutputStream(logFile);
        if (out)
            *out << logStream.str();
        else
            std::cerr << "Unable to open log file at " << logFile << " to write to" << std::endl;
    }

    return 0;
}
