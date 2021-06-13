/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"
#include <unistd.h>

using std::chrono::duration_cast;
using std::chrono::milliseconds;

#define MAX_CALLBACKS 32

using namespace std;

typedef struct {
    log_LogFn fn;
    FILE *udata;
    int level;
} Callback;

static struct {
    void *udata;
    log_LockFn lock;
    int level;
    bool quiet;
    Callback callbacks[MAX_CALLBACKS];
} L;

static int nesting = 0;
static auto _map = std::map<string , long>();


static const char *level_strings[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

std::string tail(std::string const& source, size_t const length) {
    if (length >= source.size()) { return source; }
    return source.substr(source.size() - length);
} // tail

static void stdout_callback(log_Event *ev) {
    auto now = duration_cast<milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';

    auto pid = to_string(getpid());
    // auto tid = to_string(pthread_self());
    auto id = pid; // + ":" + tid;

    auto fmtStr = string(ev->fmt);
    auto type = fmtStr.length() >= 3 ? fmtStr.substr(0, 3) : "";

    auto fmtKey = fmtStr.substr(type.length(), fmtStr.length() - type.length());
    long stopwatch = -1;

    if (type == "<- ") {
        nesting--;
        if (_map[fmtKey]) {
            stopwatch = now - _map[fmtKey];
            _map.erase(fmtKey);
        }
    }

    auto stopwatchStr = stopwatch == -1 ? "" : to_string(stopwatch) + " ms";
    auto stopwatchSpacerSize = 8 - stopwatchStr.length();
    auto stopwatchSpacer = stopwatchSpacerSize > 0 ? std::string(stopwatchSpacerSize, ' ') : "";
    stopwatchStr = stopwatchSpacer + stopwatchStr;

    auto lineNoSpacerSize = 4 - to_string(ev->line).length();
    auto lineNoSpacer = lineNoSpacerSize > 0 ? std::string(lineNoSpacerSize, ' ') : "";
    auto lineNo = lineNoSpacer + to_string(ev->line);

    auto indention = nesting * 3 - (type == "   " ? 3 : 0);
    auto indentionSpacer = std::string(indention, ' ');
    auto file = tail(string(ev->file), 32);

    fprintf(ev->udata, "%s | %s | %-5s | %s:%s | %s | %s ",
            buf, id.c_str(), level_strings[ev->level], file.c_str(), lineNo.c_str(), stopwatchStr.c_str(), indentionSpacer.c_str());

    if (type == "-> ") {
        auto p = pair<string, ulong>(fmtKey, now);
        _map.insert(p);

        nesting++;
    }

    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

static void lock(void) {
    if (L.lock) { L.lock(true, L.udata); }
}


static void unlock(void) {
    if (L.lock) { L.lock(false, L.udata); }
}


const char *log_level_string(int level) {
    return level_strings[level];
}


void log_set_lock(log_LockFn fn, void *udata) {
    L.lock = fn;
    L.udata = udata;
}


void log_set_level(int level) {
    L.level = level;
}


void log_set_quiet(bool enable) {
    L.quiet = enable;
}


int log_add_callback(log_LogFn fn, FILE *udata, int level) {
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!L.callbacks[i].fn) {
            L.callbacks[i] = (Callback) {fn, udata, level};
            return 0;
        }
    }
    return -1;
}


static void init_event(log_Event *ev, FILE *udata) {
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}


void log_log(int level, const char *file, int line, const char *fmt, ...) {
    log_Event ev = {
        .fmt   = fmt,
        .file  = file,
        .line  = line,
        .level = level
    };

    lock();

    if (!L.quiet && level >= L.level) {
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }

    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        Callback *cb = &L.callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }

    unlock();
}
