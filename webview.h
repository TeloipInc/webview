/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef WEBVIEW_H
#define WEBVIEW_H

#ifndef WEBVIEW_API
#define WEBVIEW_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *webview_t;

// Creates a new webview instance. Window parameter can be a pointer to the 
// native window handle. If it's non-null - then child WebView will be embedded 
// into the given parent window. Otherwise a new window will be created.
// Depending on the platform, a GtkWindow, NSWindow or HWND pointer can be
// passed here.
WEBVIEW_API webview_t webview_create(void *window);

// Destroys a webview and closes the native window.
WEBVIEW_API void webview_destroy(webview_t w);

// Adaptiv Networks additions vv

// Returns non-zero if webview_addview and subsequent webview calls need to 
// happen on the same thread where webview_run will be called. 
WEBVIEW_API int webview_init_in_run_thread(webview_t w);

// Set whether the window close button hides (true) or not.
WEBVIEW_API void webview_set_hide_on_close(webview_t w, int hide_on_close);

// Adds a hidden webview to the window. If debug is non-zero - developer tools will
// be enabled (if the platform supports them).
WEBVIEW_API void webview_addview(webview_t w, int debug);

// Shows the webview window.
WEBVIEW_API void webview_show(webview_t w);

// Hides the webview window.
WEBVIEW_API void webview_hide(webview_t w);

// Sets the window icon using image data from the specified file. Windows only.
WEBVIEW_API void webview_set_window_icon_from_file(webview_t w, const char *filename);

// Set the menu item callback to 'MenuItemCallback:'. Darwin/macOS only.
WEBVIEW_API void webview_set_callback_method(webview_t w);

// Adaptiv Networks additions ^^

// Runs the main loop until it's terminated. After this function exits - you
// must destroy the webview.
WEBVIEW_API void webview_run(webview_t w);

// Stops the main loop. It is safe to call this function from another other
// background thread.
WEBVIEW_API void webview_terminate(webview_t w);

// Posts a function to be executed on the main thread. You normally do not need
// to call this function, unless you want to tweak the native window.
WEBVIEW_API void
webview_dispatch(webview_t w, void (*fn)(webview_t w, void *arg), void *arg);

// Returns a native window handle pointer. When using GTK backend the pointer
// is GtkWindow pointer, when using Cocoa backend the pointer is NSWindow
// pointer, when using Win32 backend the pointer is HWND pointer.
WEBVIEW_API void *webview_get_window(webview_t w);

// Updates the title of the native window. Must be called from the UI thread.
WEBVIEW_API void webview_set_title(webview_t w, const char *title);

// Window size hints
#define WEBVIEW_HINT_NONE 0  // Width and height are default size
#define WEBVIEW_HINT_MIN 1   // Width and height are minimum bounds
#define WEBVIEW_HINT_MAX 2   // Width and height are maximum bounds
#define WEBVIEW_HINT_FIXED 3 // Window size can not be changed by a user
// Updates native window size. See WEBVIEW_HINT constants.
WEBVIEW_API void webview_set_size(webview_t w, int width, int height,
                                  int hints);

// Navigates webview to the given URL. URL may be a data URI, i.e.
// "data:text/text,<html>...</html>". It is often ok not to url-encode it
// properly, webview will re-encode it for you.
WEBVIEW_API void webview_navigate(webview_t w, const char *url);

// Injects JavaScript code at the initialization of the new page. Every time
// the webview will open a the new page - this initialization code will be
// executed. It is guaranteed that code is executed before window.onload.
WEBVIEW_API void webview_init(webview_t w, const char *js);

// Evaluates arbitrary JavaScript code. Evaluation happens asynchronously, also
// the result of the expression is ignored. Use RPC bindings if you want to
// receive notifications about the results of the evaluation.
WEBVIEW_API void webview_eval(webview_t w, const char *js);

// Binds a native C callback so that it will appear under the given name as a
// global JavaScript function. Internally it uses webview_init(). Callback
// receives a request string and a user-provided argument pointer. Request
// string is a JSON array of all the arguments passed to the JavaScript
// function.
WEBVIEW_API void webview_bind(webview_t w, const char *name,
                              void (*fn)(const char *seq, const char *req,
                                         void *arg),
                              void *arg);

// Allows to return a value from the native binding. Original request pointer
// must be provided to help internal RPC engine match requests with responses.
// If status is zero - result is expected to be a valid JSON result value.
// If status is not zero - result is an error JSON object.
WEBVIEW_API void webview_return(webview_t w, const char *seq, int status,
                                const char *result);

#ifdef __cplusplus
}
#endif

#ifndef WEBVIEW_HEADER

#if !defined(WEBVIEW_GTK) && !defined(WEBVIEW_COCOA) && !defined(WEBVIEW_EDGE)
#if defined(__linux__)
#define WEBVIEW_GTK
#elif defined(__APPLE__)
#define WEBVIEW_COCOA
#elif defined(_WIN32)
#define WEBVIEW_EDGE
#else
#error "please, specify webview backend"
#endif
#endif

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <cstring>

namespace webview {
using dispatch_fn_t = std::function<void()>;

// Convert ASCII hex digit to a nibble (four bits, 0 - 15).
//
// Use unsigned to avoid signed overflow UB.
static inline unsigned char hex2nibble(unsigned char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  } else if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return 0;
}

// Convert ASCII hex string (two characters) to byte.
//
// E.g., "0B" => 0x0B, "af" => 0xAF.
static inline char hex2char(const char *p) {
  return hex2nibble(p[0]) * 16 + hex2nibble(p[1]);
}

inline std::string url_encode(const std::string s) {
  std::string encoded;
  for (unsigned int i = 0; i < s.length(); i++) {
    auto c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded = encoded + c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02x", c);
      encoded = encoded + hex;
    }
  }
  return encoded;
}

inline std::string url_decode(const std::string st) {
  std::string decoded;
  const char *s = st.c_str();
  size_t length = strlen(s);
  for (unsigned int i = 0; i < length; i++) {
    if (s[i] == '%') {
      decoded.push_back(hex2char(s + i + 1));
      i = i + 2;
    } else if (s[i] == '+') {
      decoded.push_back(' ');
    } else {
      decoded.push_back(s[i]);
    }
  }
  return decoded;
}

inline std::string html_from_uri(const std::string s) {
  if (s.substr(0, 15) == "data:text/html,") {
    return url_decode(s.substr(15));
  }
  return "";
}

inline int json_parse_c(const char *s, size_t sz, const char *key, size_t keysz,
                        const char **value, size_t *valuesz) {
  enum {
    JSON_STATE_VALUE,
    JSON_STATE_LITERAL,
    JSON_STATE_STRING,
    JSON_STATE_ESCAPE,
    JSON_STATE_UTF8
  } state = JSON_STATE_VALUE;
  const char *k = NULL;
  int index = 1;
  int depth = 0;
  int utf8_bytes = 0;

  if (key == NULL) {
    index = keysz;
    keysz = 0;
  }

  *value = NULL;
  *valuesz = 0;

  for (; sz > 0; s++, sz--) {
    enum {
      JSON_ACTION_NONE,
      JSON_ACTION_START,
      JSON_ACTION_END,
      JSON_ACTION_START_STRUCT,
      JSON_ACTION_END_STRUCT
    } action = JSON_ACTION_NONE;
    unsigned char c = *s;
    switch (state) {
    case JSON_STATE_VALUE:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ':') {
        continue;
      } else if (c == '"') {
        action = JSON_ACTION_START;
        state = JSON_STATE_STRING;
      } else if (c == '{' || c == '[') {
        action = JSON_ACTION_START_STRUCT;
      } else if (c == '}' || c == ']') {
        action = JSON_ACTION_END_STRUCT;
      } else if (c == 't' || c == 'f' || c == 'n' || c == '-' ||
                 (c >= '0' && c <= '9')) {
        action = JSON_ACTION_START;
        state = JSON_STATE_LITERAL;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_LITERAL:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ']' || c == '}' || c == ':') {
        state = JSON_STATE_VALUE;
        s--;
        sz++;
        action = JSON_ACTION_END;
      } else if (c < 32 || c > 126) {
        return -1;
      } // fallthrough
    case JSON_STATE_STRING:
      if (c < 32 || (c > 126 && c < 192)) {
        return -1;
      } else if (c == '"') {
        action = JSON_ACTION_END;
        state = JSON_STATE_VALUE;
      } else if (c == '\\') {
        state = JSON_STATE_ESCAPE;
      } else if (c >= 192 && c < 224) {
        utf8_bytes = 1;
        state = JSON_STATE_UTF8;
      } else if (c >= 224 && c < 240) {
        utf8_bytes = 2;
        state = JSON_STATE_UTF8;
      } else if (c >= 240 && c < 247) {
        utf8_bytes = 3;
        state = JSON_STATE_UTF8;
      } else if (c >= 128 && c < 192) {
        return -1;
      }
      break;
    case JSON_STATE_ESCAPE:
      if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' ||
          c == 'n' || c == 'r' || c == 't' || c == 'u') {
        state = JSON_STATE_STRING;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_UTF8:
      if (c < 128 || c > 191) {
        return -1;
      }
      utf8_bytes--;
      if (utf8_bytes == 0) {
        state = JSON_STATE_STRING;
      }
      break;
    default:
      return -1;
    }

    if (action == JSON_ACTION_END_STRUCT) {
      depth--;
    }

    if (depth == 1) {
      if (action == JSON_ACTION_START || action == JSON_ACTION_START_STRUCT) {
        if (index == 0) {
          *value = s;
        } else if (keysz > 0 && index == 1) {
          k = s;
        } else {
          index--;
        }
      } else if (action == JSON_ACTION_END ||
                 action == JSON_ACTION_END_STRUCT) {
        if (*value != NULL && index == 0) {
          *valuesz = (size_t)(s + 1 - *value);
          return 0;
        } else if (keysz > 0 && k != NULL) {
          if (keysz == (size_t)(s - k - 1) && memcmp(key, k + 1, keysz) == 0) {
            index = 0;
          } else {
            index = 2;
          }
          k = NULL;
        }
      }
    }

    if (action == JSON_ACTION_START_STRUCT) {
      depth++;
    }
  }
  return -1;
}

inline std::string json_escape(std::string s) {
  // TODO: implement
  return '"' + s + '"';
}

inline int json_unescape(const char *s, size_t n, char *out) {
  int r = 0;
  if (*s++ != '"') {
    return -1;
  }
  while (n > 2) {
    char c = *s;
    if (c == '\\') {
      s++;
      n--;
      switch (*s) {
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case '\"':
        c = '\"';
        break;
      default: // TODO: support unicode decoding
        return -1;
      }
    }
    if (out != NULL) {
      *out++ = c;
    }
    s++;
    n--;
    r++;
  }
  if (*s != '"') {
    return -1;
  }
  if (out != NULL) {
    *out = '\0';
  }
  return r;
}

inline std::string json_parse(const std::string s, const std::string key,
                              const int index) {
  const char *value;
  size_t value_sz;
  if (key == "") {
    json_parse_c(s.c_str(), s.length(), nullptr, index, &value, &value_sz);
  } else {
    json_parse_c(s.c_str(), s.length(), key.c_str(), key.length(), &value,
                 &value_sz);
  }
  if (value != nullptr) {
    if (value[0] != '"') {
      return std::string(value, value_sz);
    }
    int n = json_unescape(value, value_sz, nullptr);
    if (n > 0) {
      char *decoded = new char[n + 1];
      json_unescape(value, value_sz, decoded);
      std::string result(decoded, n);
      delete[] decoded;
      return result;
    }
  }
  return "";
}

} // namespace webview

#if defined(WEBVIEW_GTK)
//
// ====================================================================
//
// This implementation uses webkit2gtk backend. It requires gtk+3.0 and
// webkit2gtk-4.0 libraries. Proper compiler flags can be retrieved via:
//
//   pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0
//
// ====================================================================
//
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <X11/Xlib.h>

namespace webview {

class gtk_webkit_engine {
public:
  gtk_webkit_engine(void *window)
      : m_window(static_cast<GtkWidget *>(window)) {
  }

  int init_in_run_thread() {
    return false;
  }

  void set_hide_on_close(bool hide_on_close) { m_hide_on_close = hide_on_close; }

  void add_view(bool debug) {
    XInitThreads();
    gtk_init_check(0, NULL);
    m_window = static_cast<GtkWidget *>(m_window);
    if (m_window == nullptr) {
      m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    }
    g_signal_connect(G_OBJECT(m_window), "destroy",
                    G_CALLBACK(+[](GtkWidget *, gpointer arg) {
                      static_cast<gtk_webkit_engine *>(arg)->terminate();
                    }),
                    this);
    g_signal_connect(G_OBJECT(m_window), "delete-event",
                    G_CALLBACK(+[](GtkWidget *, GdkEvent  *, gpointer arg) -> gboolean {
                      return static_cast<gtk_webkit_engine *>(arg)->on_window_close();
                    }),
                    this);

    // Initialize webview widget
    m_webview = webkit_web_view_new();
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    g_signal_connect(manager, "script-message-received::external",
                    G_CALLBACK(+[](WebKitUserContentManager *,
                                    WebKitJavascriptResult *r, gpointer arg) {
                      auto *w = static_cast<gtk_webkit_engine *>(arg);
#if WEBKIT_MAJOR_VERSION >= 2 && WEBKIT_MINOR_VERSION >= 22
                      JSCValue *value =
                          webkit_javascript_result_get_js_value(r);
                      char *s = jsc_value_to_string(value);
#else
                      JSGlobalContextRef ctx =
                          webkit_javascript_result_get_global_context(r);
                      JSValueRef value = webkit_javascript_result_get_value(r);
                      JSStringRef js = JSValueToStringCopy(ctx, value, NULL);
                      size_t n = JSStringGetMaximumUTF8CStringSize(js);
                      char *s = g_new(char, n);
                      JSStringGetUTF8CString(js, s, n);
                      JSStringRelease(js);
#endif
                      w->on_message(s);
                      g_free(s);
                    }),
                    this);
    webkit_user_content_manager_register_script_message_handler(manager,
                                                                "external");
    init("window.external={invoke:function(s){window.webkit.messageHandlers."
        "external.postMessage(s);}}");

    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_webview));
    gtk_widget_grab_focus(GTK_WIDGET(m_webview));

    WebKitSettings *settings =
        webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webview));
    webkit_settings_set_javascript_can_access_clipboard(settings, true);
    if (debug) {
      webkit_settings_set_enable_write_console_messages_to_stdout(settings,
                                                                  true);
      webkit_settings_set_enable_developer_extras(settings, true);
    }
  }

  void show() { gtk_widget_show_all(m_window); }
  void hide() { gtk_widget_hide(m_window); }
  void *window() { return (void *)m_window; }
  void run() { gtk_main(); }
  void terminate() { gtk_main_quit(); }
  void dispatch(std::function<void()> f) {
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, (GSourceFunc)([](void *f) -> int {
                      (*static_cast<dispatch_fn_t *>(f))();
                      return G_SOURCE_REMOVE;
                    }),
                    new std::function<void()>(f),
                    [](void *f) { delete static_cast<dispatch_fn_t *>(f); });
  }

  void set_title(const std::string title) {
    gtk_window_set_title(GTK_WINDOW(m_window), title.c_str());
  }

  void set_size(int width, int height, int hints) {
    gtk_window_set_resizable(GTK_WINDOW(m_window), hints != WEBVIEW_HINT_FIXED);
    if (hints == WEBVIEW_HINT_NONE) {
      gtk_window_resize(GTK_WINDOW(m_window), width, height);
    } else if (hints == WEBVIEW_HINT_FIXED) {
      gtk_widget_set_size_request(m_window, width, height);
    } else {
      GdkGeometry g;
      g.min_width = g.max_width = width;
      g.min_height = g.max_height = height;
      GdkWindowHints h =
          (hints == WEBVIEW_HINT_MIN ? GDK_HINT_MIN_SIZE : GDK_HINT_MAX_SIZE);
      // This defines either MIN_SIZE, or MAX_SIZE, but not both:
      gtk_window_set_geometry_hints(GTK_WINDOW(m_window), nullptr, &g, h);
    }
  }

  void navigate(const std::string url) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), url.c_str());
  }

  void init(const std::string js) {
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(
            WEBKIT_WEB_VIEW(m_webview));
    webkit_user_content_manager_add_script(
        manager,
        webkit_user_script_new(
            js.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL,
            NULL));
  }

  void eval(const std::string js) {
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(m_webview), js.c_str(), -1, NULL, NULL, NULL, NULL, NULL);
  }

private:
  gboolean on_window_close() {
    if (m_hide_on_close) {
      return gtk_widget_hide_on_delete(m_window);
    }
    return FALSE;
  }

  virtual void on_message(const std::string msg) = 0;
  bool m_hide_on_close;
  GtkWidget *m_window;
  GtkWidget *m_webview;
};

using browser_engine = gtk_webkit_engine;

} // namespace webview

#elif defined(WEBVIEW_COCOA)

//
// ====================================================================
//
// This implementation uses Cocoa WKWebView backend on macOS. It is
// written using ObjC runtime and uses WKWebView class as a browser runtime.
// You should pass "-framework Webkit" flag to the compiler.
//
// ====================================================================
//

#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc-runtime.h>

#define NSBackingStoreBuffered 2

#define NSWindowStyleMaskResizable 8
#define NSWindowStyleMaskMiniaturizable 4
#define NSWindowStyleMaskTitled 1
#define NSWindowStyleMaskClosable 2

#define NSApplicationActivationPolicyRegular 0
#define NSApplicationActivationPolicyAccessory 1

#define WKUserScriptInjectionTimeAtDocumentStart 0

namespace webview {

// Helpers to avoid too much typing
id operator"" _cls(const char *s, std::size_t) { return (id)objc_getClass(s); }
SEL operator"" _sel(const char *s, std::size_t) { return sel_registerName(s); }
id operator"" _str(const char *s, std::size_t) {
  return ((id(*)(id, SEL, const char *))objc_msgSend)(
      "NSString"_cls, "stringWithUTF8String:"_sel, s);
}

// Adaptiv Networks additions vv
extern "C" void menuItemCallback(uintptr_t ident);
// Adaptiv Networks additions ^^

class cocoa_wkwebview_engine {
public:
  cocoa_wkwebview_engine(void *window) : m_window(static_cast<id>(window)){}

  int init_in_run_thread() {
    return true;
  }

  void add_view(bool debug) {
    // Application
    id app = ((id(*)(id, SEL))objc_msgSend)("NSApplication"_cls,
                                            "sharedApplication"_sel);
    long app_policy;
    if (m_hide_on_close) {
      // 'Accessory':
      // - No menu bar menus (but still allows systray / menu status item).
      // - Not in Dock or command-tab switcher.
      app_policy = NSApplicationActivationPolicyAccessory;
    } else {
      // 'Normal' application:
      // - Has own menus (at least application).
      // - Appears in Dock.
      // - Appears in command-tab switcher.
      app_policy = NSApplicationActivationPolicyRegular;
    }
    ((void (*)(id, SEL, long))objc_msgSend)(
        app, "setActivationPolicy:"_sel, app_policy);

    // Delegate
    auto cls =
        objc_allocateClassPair((Class) "NSResponder"_cls, "AppDelegate", 0);
    class_addProtocol(cls, objc_getProtocol("NSTouchBarProvider"));
    class_addMethod(cls, "applicationShouldTerminateAfterLastWindowClosed:"_sel,
                    (IMP)(+[](id, SEL, id) -> BOOL { return 1; }), "c@:@");
    class_addMethod(cls, "userContentController:didReceiveScriptMessage:"_sel,
                    (IMP)(+[](id self, SEL, id, id msg) {
                      auto w =
                          (cocoa_wkwebview_engine *)objc_getAssociatedObject(
                              self, "webview");
                      assert(w);
                      w->on_message(((const char *(*)(id, SEL))objc_msgSend)(
                          ((id(*)(id, SEL))objc_msgSend)(msg, "body"_sel),
                          "UTF8String"_sel));
                    }),
                    "v@:@@");
    objc_registerClassPair(cls);

    auto delegate = ((id(*)(id, SEL))objc_msgSend)((id)cls, "new"_sel);
    objc_setAssociatedObject(delegate, "webview", (id)this,
                             OBJC_ASSOCIATION_ASSIGN);
    ((void (*)(id, SEL, id))objc_msgSend)(app, sel_registerName("setDelegate:"),
                                          delegate);

    // Main window
    if (m_window == nullptr) {
      m_window = ((id(*)(id, SEL))objc_msgSend)("NSWindow"_cls, "alloc"_sel);
      m_window =
          ((id(*)(id, SEL, CGRect, int, unsigned long, int))objc_msgSend)(
              m_window, "initWithContentRect:styleMask:backing:defer:"_sel,
              CGRectMake(0, 0, 0, 0), 0, NSBackingStoreBuffered, 0);
    }

    override_close_button(); // Adaptiv Network addition

    // Webview
    auto config =
        ((id(*)(id, SEL))objc_msgSend)("WKWebViewConfiguration"_cls, "new"_sel);
    m_manager =
        ((id(*)(id, SEL))objc_msgSend)(config, "userContentController"_sel);
    m_webview = ((id(*)(id, SEL))objc_msgSend)("WKWebView"_cls, "alloc"_sel);

    if (debug) {
      // Equivalent Obj-C:
      // [[config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
      ((id(*)(id, SEL, id, id))objc_msgSend)(
          ((id(*)(id, SEL))objc_msgSend)(config, "preferences"_sel),
          "setValue:forKey:"_sel,
          ((id(*)(id, SEL, BOOL))objc_msgSend)("NSNumber"_cls,
                                               "numberWithBool:"_sel, 1),
          "developerExtrasEnabled"_str);
    }

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"fullScreenEnabled"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, "preferences"_sel),
        "setValue:forKey:"_sel,
        ((id(*)(id, SEL, BOOL))objc_msgSend)("NSNumber"_cls,
                                             "numberWithBool:"_sel, 1),
        "fullScreenEnabled"_str);

    // Adaptiv Networks addition: fix keyboard field navigation (tab, shift-tab,
    // space, return, etc) that already work for Windows & Linux.
    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"tabFocusesLinks"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, "preferences"_sel),
        "setValue:forKey:"_sel,
        ((id(*)(id, SEL, BOOL))objc_msgSend)("NSNumber"_cls,
                                             "numberWithBool:"_sel, 1),
        "tabFocusesLinks"_str);

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"javaScriptCanAccessClipboard"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, "preferences"_sel),
        "setValue:forKey:"_sel,
        ((id(*)(id, SEL, BOOL))objc_msgSend)("NSNumber"_cls,
                                             "numberWithBool:"_sel, 1),
        "javaScriptCanAccessClipboard"_str);

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"DOMPasteAllowed"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, "preferences"_sel),
        "setValue:forKey:"_sel,
        ((id(*)(id, SEL, BOOL))objc_msgSend)("NSNumber"_cls,
                                             "numberWithBool:"_sel, 1),
        "DOMPasteAllowed"_str);

    ((void (*)(id, SEL, CGRect, id))objc_msgSend)(
        m_webview, "initWithFrame:configuration:"_sel, CGRectMake(0, 0, 0, 0),
        config);
    ((void (*)(id, SEL, id, id))objc_msgSend)(
        m_manager, "addScriptMessageHandler:name:"_sel, delegate,
        "external"_str);

    init(R"script(
                      window.external = {
                        invoke: function(s) {
                          window.webkit.messageHandlers.external.postMessage(s);
                        },
                      };
                     )script");
    ((void (*)(id, SEL, id))objc_msgSend)(m_window, "setContentView:"_sel,
                                          m_webview);
    ((void (*)(id, SEL, id))objc_msgSend)(m_window, "makeKeyAndOrderFront:"_sel,
                                          nullptr);

    // Adaptiv Networks additions vv

    m_appdel_cls = cls;

    // Seem to need an initial 'hide:' before the GUI is shown. Without this, if
    // in 'hide on close' mode, if the user closes (hides) the window and then
    // quits the application (without any intervening window hide/shows), then
    // the application will crash. This avoids this.
    // Hide the application (& window): [app hide:app]
    ((void (*)(id, SEL, id))objc_msgSend)(app,
        "hide:"_sel,
        app);

    // Adaptiv Networks additions ^^
  }
  ~cocoa_wkwebview_engine() { close(); }

  void *window() { return (void *)m_window; }

  void terminate() {
    // Adaptiv Networks additions vv
    // Application won't exit if the window is hidden or miniaturized to the
    // Dock.
    restore_window_timeout(4*1000); // Wait up to 4 s.
    // Note that 'terminate:' below causes the event loop to stop and the
    // application to exit. The 'run' method will not exit and control will not
    // return to it's caller. Experiments with 'stop:' also did not rectify
    // this. Further cleanup code can be put in applicationWillTerminate: and/or
    // its siblings.
    // Adaptiv Networks additions ^^
    close();
    ((void (*)(id, SEL, id))objc_msgSend)("NSApp"_cls, "terminate:"_sel,
                                          nullptr);
  }
  void run() {
    id app = ((id(*)(id, SEL))objc_msgSend)("NSApplication"_cls,
                                            "sharedApplication"_sel);
    dispatch([&]() {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(
          app, "activateIgnoringOtherApps:"_sel, 1);
    });
    ((void (*)(id, SEL))objc_msgSend)(app, "run"_sel);
  }
  void dispatch(std::function<void()> f) {
    dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                     (dispatch_function_t)([](void *arg) {
                       auto f = static_cast<dispatch_fn_t *>(arg);
                       (*f)();
                       delete f;
                     }));
  }
  void set_title(const std::string title) {
    ((void (*)(id, SEL, id))objc_msgSend)(
        m_window, "setTitle:"_sel,
        ((id(*)(id, SEL, const char *))objc_msgSend)(
            "NSString"_cls, "stringWithUTF8String:"_sel, title.c_str()));
  }
  void set_size(int width, int height, int hints) {
    auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                 NSWindowStyleMaskMiniaturizable;
    if (hints != WEBVIEW_HINT_FIXED) {
      style = style | NSWindowStyleMaskResizable;
    }
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(
        m_window, "setStyleMask:"_sel, style);

    if (hints == WEBVIEW_HINT_MIN) {
      ((void (*)(id, SEL, CGSize))objc_msgSend)(
          m_window, "setContentMinSize:"_sel, CGSizeMake(width, height));
    } else if (hints == WEBVIEW_HINT_MAX) {
      ((void (*)(id, SEL, CGSize))objc_msgSend)(
          m_window, "setContentMaxSize:"_sel, CGSizeMake(width, height));
    } else {
      ((void (*)(id, SEL, CGRect, BOOL, BOOL))objc_msgSend)(
          m_window, "setFrame:display:animate:"_sel,
          CGRectMake(0, 0, width, height), 1, 0);
    }
    ((void (*)(id, SEL))objc_msgSend)(m_window, "center"_sel);
  }
  void navigate(const std::string url) {
    auto nsurl = ((id(*)(id, SEL, id))objc_msgSend)(
        "NSURL"_cls, "URLWithString:"_sel,
        ((id(*)(id, SEL, const char *))objc_msgSend)(
            "NSString"_cls, "stringWithUTF8String:"_sel, url.c_str()));

    ((void (*)(id, SEL, id))objc_msgSend)(
        m_webview, "loadRequest:"_sel,
        ((id(*)(id, SEL, id))objc_msgSend)("NSURLRequest"_cls,
                                           "requestWithURL:"_sel, nsurl));
  }
  void init(const std::string js) {
    // Equivalent Obj-C:
    // [m_manager addUserScript:[[WKUserScript alloc] initWithSource:[NSString stringWithUTF8String:js.c_str()] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]]
    ((void (*)(id, SEL, id))objc_msgSend)(
        m_manager, "addUserScript:"_sel,
        ((id(*)(id, SEL, id, long, BOOL))objc_msgSend)(
            ((id(*)(id, SEL))objc_msgSend)("WKUserScript"_cls, "alloc"_sel),
            "initWithSource:injectionTime:forMainFrameOnly:"_sel,
            ((id(*)(id, SEL, const char *))objc_msgSend)(
                "NSString"_cls, "stringWithUTF8String:"_sel, js.c_str()),
            WKUserScriptInjectionTimeAtDocumentStart, 1));
  }
  void eval(const std::string js) {
    ((void (*)(id, SEL, id, id))objc_msgSend)(
        m_webview, "evaluateJavaScript:completionHandler:"_sel,
        ((id(*)(id, SEL, const char *))objc_msgSend)(
            "NSString"_cls, "stringWithUTF8String:"_sel, js.c_str()),
        nullptr);
  }

  // Adaptiv Networks additions vv

public:
  void set_callback_method() {
    class_addMethod(m_appdel_cls,
                    "MenuItemCallback:"_sel,
                    (IMP)(+[](id self, SEL, id sender) {
                      menuItemCallback(reinterpret_cast<uintptr_t>(sender));
                    }),
                    "v@:@");
  }

  void set_hide_on_close(bool hide_on_close) {
    m_hide_on_close = hide_on_close;
  }

  void show() {
    show_window();
  }

  void hide() {
    hide_window();
  }

private:
  // Override window close (red 'x') button to hide instead.
  void override_close_button() {
    // NSWindowDelegate WindowDelegate for 'windowShouldClose:'
    // Create delegate class 'WindowDelegate'.
    auto windel_cls = objc_allocateClassPair((Class) "NSObject"_cls, "WindowDelegate", 0);
    class_addProtocol(windel_cls, objc_getProtocol("NSWindowDelegate"));
    // Add 'windowShouldClose:' method to class.
    class_addMethod(windel_cls, "windowShouldClose:"_sel,
                    (IMP)(+[](id self, SEL, id) -> BOOL {
                        // Get the object (this) pointer.
                        auto w = (cocoa_wkwebview_engine *)objc_getAssociatedObject(
                            self,
                            "webview");
                        assert(w);

                        if (w->m_hide_on_close) {
                          // Hide the application/window: quit is via systray or
                          // other.
                          w->hide_window();
                          return 0; // Don't allow window close.
                        }
                        // else: allow window to close and application exit.
                        return 1; // Allow window close.
                    }),
                    "B@:@");
    objc_registerClassPair(windel_cls);

    // windel = [WindowDelegate new] = [[WindowDelegate alloc] init]
    auto windel = ((id(*)(id, SEL))objc_msgSend)((id)windel_cls, "new"_sel);

    // Associate 'this' pointer for retrieval in windowShouldClose method.
    objc_setAssociatedObject(windel, "webview", (id)this, OBJC_ASSOCIATION_ASSIGN);

    // [m_window setDelegate:windel]
    ((void (*)(id, SEL, id))objc_msgSend)(m_window, "setDelegate:"_sel,
                                          windel);
  }

  // Make the application & window visible.
  void show_window() {
    // app = [NSApplication sharedApplication]
    id app = ((id(*)(id, SEL))objc_msgSend)("NSApplication"_cls,
        "sharedApplication"_sel);
    assert(app);

    // Unhide the application (& window): [app unhide:app]
    ((void (*)(id, SEL, id))objc_msgSend)(app,
        "unhide:"_sel,
        app);

    // Bring application to the front: [app activateIgnoringOtherApps:YES]
    ((void (*)(id, SEL, BOOL))objc_msgSend)(app,
        "activateIgnoringOtherApps:"_sel,
        1);

    // Deminiaturize in case the window was minimized to the Dock by user.
    // [m_window deminiaturize:m_window]
    ((void (*)(id, SEL, id))objc_msgSend)(m_window,
        "deminiaturize:"_sel,
        m_window);
  }

  // Hide the application & window.
  void hide_window() {
    // app = [NSApplication sharedApplication]
    id app = ((id(*)(id, SEL))objc_msgSend)("NSApplication"_cls,
        "sharedApplication"_sel);
    assert(app);

    // Hide the application (& window): [app hide:app]
    ((void (*)(id, SEL, id))objc_msgSend)(app,
        "hide:"_sel,
        app);
  }

  // Restore the window and wait until it is the main window. Will unminiaturize
  // window from Dock, if required. Will wait at most 'timeout_ms', or
  // indefinitely if timeout_ms = 0.
  void restore_window_timeout(unsigned int timeout_ms) {
    BOOL is_min;
    BOOL is_main;

    // is_min = [m_window isMiniaturized]
    is_min = ((BOOL (*)(id, SEL))objc_msgSend)(m_window,
        "isMiniaturized"_sel);
    is_main = ((BOOL (*)(id, SEL))objc_msgSend)(m_window,
            "isMainWindow"_sel);

    if (is_min || !is_main) {
      // If the window is miniaturized (minimized to the Dock) or is hidden
      // (not main window), need to restore the window (unminiaturize and
      // unhide) and wait until it becomes the main window. Failure to do either
      // of these results in window close() not working and the application will
      // not terminate (and the window and tray menu will stay around).
      // Times measured on a development system:
      // If miniaturized: about 450 ms.
      // If hidden: < 10 ms.

      show_window();

      const unsigned int pause_ms = 10;
      const unsigned int max_count = timeout_ms / pause_ms;
      unsigned int count = 0;
      do {
        usleep(pause_ms*1000);
        is_main = ((BOOL (*)(id, SEL))objc_msgSend)(m_window,
            "isMainWindow"_sel);
        ++count;
        // Wait until main window, or too timeout (and hope for the best).
      } while (!is_main && ((timeout_ms == 0) || (count < max_count)));
    }
  }

  // Adaptiv Networks additions ^^

private:
  virtual void on_message(const std::string msg) = 0;
  void close() { ((void (*)(id, SEL))objc_msgSend)(m_window, "close"_sel); }
  id m_window;
  id m_webview;
  id m_manager;

  // Adaptiv Networks additions vv
  bool m_hide_on_close;
  Class m_appdel_cls;
  // Adaptiv Networks additions ^^
};

using browser_engine = cocoa_wkwebview_engine;

} // namespace webview

#elif defined(WEBVIEW_EDGE)

//
// ====================================================================
//
// This implementation uses Win32 API to create a native window. It can
// use either EdgeHTML or Edge/Chromium backend as a browser engine.
//
// ====================================================================
//

#define WIN32_LEAN_AND_MEAN
#include <codecvt>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shlwapi.lib")

// EdgeHTML headers and libs
#include <objbase.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>
#pragma comment(lib, "windowsapp")

// Edge/Chromium headers and libs
#include "webview2.h"
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace webview {

using msg_cb_t = std::function<void(const std::string)>;

// Common interface for EdgeHTML and Edge/Chromium
class browser {
public:
  virtual ~browser() = default;
  virtual bool embed(HWND, bool, msg_cb_t) = 0;
  virtual void navigate(const std::string url) = 0;
  virtual void eval(const std::string js) = 0;
  virtual void init(const std::string js) = 0;
  virtual void move(HWND) = 0;
  virtual void resize(HWND) = 0;
};

//
// EdgeHTML browser engine
//
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI;
using namespace Windows::Web::UI::Interop;

class edge_html : public browser {
public:
  bool embed(HWND wnd, bool debug, msg_cb_t cb) override {
    init_apartment(winrt::apartment_type::single_threaded);
    auto process = WebViewControlProcess();
    auto op = process.CreateWebViewControlAsync(reinterpret_cast<int64_t>(wnd),
                                                Rect());
    if (op.Status() != AsyncStatus::Completed) {
      handle h(CreateEvent(nullptr, false, false, nullptr));
      op.Completed([h = h.get()](auto, auto) { SetEvent(h); });
      HANDLE hs[] = {h.get()};
      DWORD i;
      CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES |
                                   COWAIT_DISPATCH_CALLS |
                                   COWAIT_INPUTAVAILABLE,
                               INFINITE, 1, hs, &i);
    }
    m_webview = op.GetResults();
    m_webview.Settings().IsScriptNotifyAllowed(true);
    m_webview.IsVisible(true);
    m_webview.ScriptNotify([=](auto const &sender, auto const &args) {
      std::string s = winrt::to_string(args.Value());
      cb(s.c_str());
    });
    m_webview.NavigationStarting([=](auto const &sender, auto const &args) {
      m_webview.AddInitializeScript(winrt::to_hstring(init_js));
    });
    init("window.external.invoke = s => window.external.notify(s)");
    return true;
  }

  void navigate(const std::string url) override {
    std::string html = html_from_uri(url);
    if (html != "") {
      m_webview.NavigateToString(winrt::to_hstring(html));
    } else {
      Uri uri(winrt::to_hstring(url));
      m_webview.Navigate(uri);
    }
  }

  void init(const std::string js) override {
    init_js = init_js + "(function(){" + js + "})();";
  }

  void eval(const std::string js) override {
    m_webview.InvokeScriptAsync(
        L"eval", single_threaded_vector<hstring>({winrt::to_hstring(js)}));
  }

  void move(HWND wnd) override {
    // don't do anything
  }

  void resize(HWND wnd) override {
    if (m_webview == nullptr) {
      return;
    }
    RECT r;
    GetClientRect(wnd, &r);
    Rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
    m_webview.Bounds(bounds);
  }

private:
  WebViewControl m_webview = nullptr;
  std::string init_js = "";
};

//
// Edge/Chromium browser engine
//
class edge_chromium : public browser {
public:
  bool embed(HWND wnd, bool debug, msg_cb_t cb) override {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    flag.test_and_set();

    wchar_t currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
    wchar_t *currentExeName = PathFindFileNameW(currentExePath);

    wchar_t dataPath[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, dataPath))) {
      return false;
    }
    wchar_t userDataFolder[MAX_PATH];
    PathCombineW(userDataFolder, dataPath, currentExeName);

    HRESULT res = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder, nullptr,
        new webview2_com_handler(wnd, cb,
                                 [&](ICoreWebView2Controller *controller) {
                                   m_controller = controller;
                                   m_controller->get_CoreWebView2(&m_webview);
                                   m_webview->AddRef();
                                   flag.clear();
                                 }));
    if (res != S_OK) {
      return false;
    }
    MSG msg = {};
    while (flag.test_and_set() && GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    return true;
  }

  void move(HWND wnd) override {
    if (m_controller == nullptr) {
      return;
    }

    // NotifyParentWindowPositionChanged lets webview know about the new 
    // location of the window
    m_controller->NotifyParentWindowPositionChanged();

    // unfortunately, combobox dropdowns that are already on screen at the time
    // of the move don't get repositioned when NotifyParentWindowPositionChanged
    // gets called. calling SetFocus(wnd) leads to a currently shown dropdown
    // lose focus and disappear from screen.
    SetFocus(wnd);
  }

  void resize(HWND wnd) override {
    if (m_controller == nullptr) {
      return;
    }
    RECT bounds;
    GetClientRect(wnd, &bounds);
    m_controller->put_Bounds(bounds);
  }

  void navigate(const std::string url) override {
    auto wurl = winrt::to_hstring(url);
    m_webview->Navigate(wurl.c_str());
  }

  void init(const std::string js) override {
    auto wjs = winrt::to_hstring(js);
    m_webview->AddScriptToExecuteOnDocumentCreated(wjs.c_str(), nullptr);
  }

  void eval(const std::string js) override {
    auto wjs = winrt::to_hstring(js);
    m_webview->ExecuteScript(wjs.c_str(), nullptr);
  }

private:
  ICoreWebView2 *m_webview = nullptr;
  ICoreWebView2Controller *m_controller = nullptr;

  class webview2_com_handler
      : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
        public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
        public ICoreWebView2WebMessageReceivedEventHandler,
        public ICoreWebView2PermissionRequestedEventHandler {
    using webview2_com_handler_cb_t =
        std::function<void(ICoreWebView2Controller *)>;

  public:
    webview2_com_handler(HWND hwnd, msg_cb_t msgCb,
                         webview2_com_handler_cb_t cb)
        : m_window(hwnd), m_msgCb(msgCb), m_cb(cb) {}
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    ULONG STDMETHODCALLTYPE Release() { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv) {
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                     ICoreWebView2Environment *env) {
      env->CreateCoreWebView2Controller(m_window, this);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                     ICoreWebView2Controller *controller) {
      controller->AddRef();

      ICoreWebView2 *webview;
      ::EventRegistrationToken token;
      controller->get_CoreWebView2(&webview);
      webview->add_WebMessageReceived(this, &token);
      webview->add_PermissionRequested(this, &token);

      m_cb(controller);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
      LPWSTR message;
      args->TryGetWebMessageAsString(&message);
      m_msgCb(winrt::to_string(message));
      sender->PostWebMessageAsString(message);

      CoTaskMemFree(message);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    Invoke(ICoreWebView2 *sender,
           ICoreWebView2PermissionRequestedEventArgs *args) {
      COREWEBVIEW2_PERMISSION_KIND kind;
      args->get_PermissionKind(&kind);
      if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
        args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
      }
      return S_OK;
    }

  private:
    HWND m_window;
    msg_cb_t m_msgCb;
    webview2_com_handler_cb_t m_cb;
  };
};

class win32_edge_engine {
public:
  win32_edge_engine(void *window) 
    : m_window(static_cast<HWND>(window)) {}

  int init_in_run_thread() {
    return true;
  }

  void set_hide_on_close(bool hide_on_close) { 
    m_hide_on_close = hide_on_close; 
  }

  void add_view(bool debug) {
    if (m_window == nullptr) {
      HINSTANCE hInstance = GetModuleHandle(nullptr);
      HICON icon = (HICON)LoadImage(
          hInstance, IDI_APPLICATION, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
          GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

      WNDCLASSEXW wc;
      ZeroMemory(&wc, sizeof(WNDCLASSEX));
      wc.cbSize = sizeof(WNDCLASSEX);
      wc.hInstance = hInstance;
      wc.lpszClassName = L"webview";
      wc.hIcon = icon;
      wc.hIconSm = icon;
      wc.lpfnWndProc =
          (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto w = (win32_edge_engine *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            switch (msg) {
            case WM_SIZE:
              w->m_browser->resize(hwnd);
              break;
            case WM_MOVE:
            case WM_MOVING:
              w->m_browser->move(hwnd);
              break;
            case WM_CLOSE:
              if (w->m_hide_on_close) {
                w->hide();
              }
              else {
                DestroyWindow(hwnd);
              }            
              break;
            case WM_DESTROY:
              w->terminate();
              break;
            case WM_GETMINMAXINFO: {
              auto lpmmi = (LPMINMAXINFO)lp;
              if (w == nullptr) {
                return 0;
              }
              if (w->m_maxsz.x > 0 && w->m_maxsz.y > 0) {
                lpmmi->ptMaxSize = w->m_maxsz;
                lpmmi->ptMaxTrackSize = w->m_maxsz;
              }
              if (w->m_minsz.x > 0 && w->m_minsz.y > 0) {
                lpmmi->ptMinTrackSize = w->m_minsz;
              }
            } break;
            default:
              return DefWindowProcW(hwnd, msg, wp, lp);
            }
            return 0;
          });
      RegisterClassExW(&wc);
      m_window = CreateWindowExW(WS_EX_TOOLWINDOW, L"webview", L"", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr,
                              nullptr, GetModuleHandle(nullptr), nullptr);
      SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    // move off screen
    RECT desktop, thisw;
    GetWindowRect(GetDesktopWindow(), &desktop);
    GetClientRect(m_window, &thisw);
    SetWindowPos(m_window, nullptr, desktop.right, desktop.bottom, thisw.right, thisw.bottom, 0);

    // the window needs to be shown on screen while browser->embed is happenning for the webview
    // component to show up properly inside the window.
    ShowWindow(m_window, SW_SHOW);

    auto cb =
        std::bind(&win32_edge_engine::on_message, this, std::placeholders::_1);

    if (!m_browser->embed(m_window, debug, cb)) {
      m_browser = std::make_unique<webview::edge_html>();
      m_browser->embed(m_window, debug, cb);
    }

    m_browser->resize(m_window);
    this->hide();

    // switch back to overlapped
    long style = GetWindowLong(m_window, GWL_EXSTYLE);
    style &= ~WS_EX_TOOLWINDOW;
    SetWindowLong(m_window, GWL_EXSTYLE, style);
  }

  void run() {
    m_main_thread = GetCurrentThreadId();

    MSG msg;
    BOOL res;
    while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
      if (msg.hwnd) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        continue;
      }
      if (msg.message == WM_APP) {
        auto f = (dispatch_fn_t *)(msg.lParam);
        (*f)();
        delete f;
      } else if (msg.message == WM_QUIT) {
        return;
      }
    }
  }
  void show() { 
    center_on_screen();

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
    SetFocus(m_window);
  }
  void hide() { ShowWindow(m_window, SW_HIDE); }
  void *window() { return (void *)m_window; }
  void terminate() { PostQuitMessage(0); }
  void dispatch(dispatch_fn_t f) {
    PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
  }

  void set_title(const std::string title) {
    SetWindowTextW(m_window, winrt::to_hstring(title).c_str());
  }

  void set_size(int width, int height, int hints) {
    auto style = GetWindowLong(m_window, GWL_STYLE);
    if (hints == WEBVIEW_HINT_FIXED) {
      style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    } else {
      style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    SetWindowLong(m_window, GWL_STYLE, style);

    if (hints == WEBVIEW_HINT_MAX) {
      m_maxsz.x = width;
      m_maxsz.y = height;
    } else if (hints == WEBVIEW_HINT_MIN) {
      m_minsz.x = width;
      m_minsz.y = height;
    } else {
      RECT r;
      r.left = r.top = 0;
      r.right = width;
      r.bottom = height;
      AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
      SetWindowPos(
          m_window, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
      m_browser->resize(m_window);
    }
  }

  void navigate(const std::string url) { m_browser->navigate(url); }
  void eval(const std::string js) { m_browser->eval(js); }
  void init(const std::string js) { m_browser->init(js); }

  void set_window_icon_from_file(const char *filename) {
    // Fetch icon of preferred size, if available. Other sizes will be scaled.
    // Observed preferred sizes of 16x16 (small) and 32x32 (big).
    HANDLE icon;

    icon = LoadImage(NULL, filename, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    if (icon) {
      SendMessageW(m_window, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }

    icon = LoadImage(NULL, filename, IMAGE_ICON, GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
    if (icon) {
      SendMessageW(m_window, WM_SETICON, ICON_BIG, (LPARAM)icon);
    }
  }

private:
  virtual void on_message(const std::string msg) = 0;

  void center_on_screen() {
    int nWidth = GetSystemMetrics(SM_CXSCREEN);
    int nHeight = GetSystemMetrics(SM_CYSCREEN);
    RECT thisw;
    GetWindowRect(m_window, &thisw);

    int width = thisw.right - thisw.left;
    int height = thisw.bottom - thisw.top;

    int xPos = (nWidth - width) / 2;
    int yPos = (nHeight - height) / 2;

    // keep within the desktop
    if (xPos < 0) xPos = 0;
    if (yPos < 0) yPos = 0;
    if (xPos + width > nWidth)   xPos = nWidth - width;
    if (yPos + height > nHeight) yPos = nHeight - height;
    SetWindowPos(m_window, nullptr, xPos, yPos, width, height, 0);
  }

  bool m_hide_on_close;
  HWND m_window;
  POINT m_minsz = POINT{0, 0};
  POINT m_maxsz = POINT{0, 0};
  DWORD m_main_thread = GetCurrentThreadId();
  std::unique_ptr<webview::browser> m_browser =
      std::make_unique<webview::edge_chromium>();
};

using browser_engine = win32_edge_engine;
} // namespace webview

#endif /* WEBVIEW_GTK, WEBVIEW_COCOA, WEBVIEW_EDGE */

namespace webview {

class webview : public browser_engine {
public:
  webview(void *wnd = nullptr)
      : browser_engine(wnd) {}

  void navigate(const std::string url) {
    if (url == "") {
      browser_engine::navigate("data:text/html," +
                               url_encode("<html><body>Hello</body></html>"));
      return;
    }
    std::string html = html_from_uri(url);
    if (html != "") {
      browser_engine::navigate("data:text/html," + url_encode(html));
    } else {
      browser_engine::navigate(url);
    }
  }

  using binding_t = std::function<void(std::string, std::string, void *)>;
  using binding_ctx_t = std::pair<binding_t *, void *>;

  using sync_binding_t = std::function<std::string(std::string)>;
  using sync_binding_ctx_t = std::pair<webview *, sync_binding_t>;

  void bind(const std::string name, sync_binding_t fn) {
    bind(
        name,
        [](std::string seq, std::string req, void *arg) {
          auto pair = static_cast<sync_binding_ctx_t *>(arg);
          pair->first->resolve(seq, 0, pair->second(req));
        },
        new sync_binding_ctx_t(this, fn));
  }

  void bind(const std::string name, binding_t f, void *arg) {
    auto js = "(function() { var name = '" + name + "';" + R"(
      var RPC = window._rpc = (window._rpc || {nextSeq: 1});
      window[name] = function() {
        var seq = RPC.nextSeq++;
        var promise = new Promise(function(resolve, reject) {
          RPC[seq] = {
            resolve: resolve,
            reject: reject,
          };
        });
        window.external.invoke(JSON.stringify({
          id: seq,
          method: name,
          params: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())";
    init(js);
    bindings[name] = new binding_ctx_t(new binding_t(f), arg);
  }

  void resolve(const std::string seq, int status, const std::string result) {
    dispatch([=]() {
      if (status == 0) {
        eval("window._rpc[" + seq + "].resolve(" + result + "); window._rpc[" +
             seq + "] = undefined");
      } else {
        eval("window._rpc[" + seq + "].reject(" + result + "); window._rpc[" +
             seq + "] = undefined");
      }
    });
  }

private:
  void on_message(const std::string msg) {
    auto seq = json_parse(msg, "id", 0);
    auto name = json_parse(msg, "method", 0);
    auto args = json_parse(msg, "params", 0);
    if (bindings.find(name) == bindings.end()) {
      return;
    }
    auto fn = bindings[name];
    (*fn->first)(seq, args, fn->second);
  }
  std::map<std::string, binding_ctx_t *> bindings;
};
} // namespace webview



WEBVIEW_API webview_t webview_create(void *wnd) {
  return new webview::webview(wnd);
}

WEBVIEW_API void webview_destroy(webview_t w) {
  delete static_cast<webview::webview *>(w);
}

// Adaptiv Networks additions vv

WEBVIEW_API int webview_init_in_run_thread(webview_t w) {
  return static_cast<webview::webview *>(w)->init_in_run_thread();
}

WEBVIEW_API void webview_set_hide_on_close(webview_t w, int hide_on_close) {
  static_cast<webview::webview *>(w)->set_hide_on_close(hide_on_close);
}

WEBVIEW_API void webview_addview(webview_t w, int debug) {
  static_cast<webview::webview *>(w)->add_view(debug);
}

WEBVIEW_API void webview_show(webview_t w) {
  static_cast<webview::webview *>(w)->show();
}

WEBVIEW_API void webview_hide(webview_t w) {
  static_cast<webview::webview *>(w)->hide();
}

// Windows only
WEBVIEW_API void webview_set_window_icon_from_file(webview_t w, const char *filename) {
#if defined(WEBVIEW_EDGE)
  static_cast<webview::webview *>(w)->set_window_icon_from_file(filename);
#endif // defined(WEBVIEW_EDGE)
}

// Darwin/macOS only
WEBVIEW_API void webview_set_callback_method(webview_t w) {
#if defined(WEBVIEW_COCOA)
  static_cast<webview::webview *>(w)->set_callback_method();
#endif // defined(WEBVIEW_COCOA)
}

// Adaptiv Networks additions ^^

WEBVIEW_API void webview_run(webview_t w) {
  static_cast<webview::webview *>(w)->run();
}

WEBVIEW_API void webview_terminate(webview_t w) {
  static_cast<webview::webview *>(w)->terminate();
}

WEBVIEW_API void webview_dispatch(webview_t w, void (*fn)(webview_t, void *),
                                  void *arg) {
  static_cast<webview::webview *>(w)->dispatch([=]() { fn(w, arg); });
}

WEBVIEW_API void *webview_get_window(webview_t w) {
  return static_cast<webview::webview *>(w)->window();
}

WEBVIEW_API void webview_set_title(webview_t w, const char *title) {
  static_cast<webview::webview *>(w)->set_title(title);
}

WEBVIEW_API void webview_set_size(webview_t w, int width, int height,
                                  int hints) {
  static_cast<webview::webview *>(w)->set_size(width, height, hints);
}

WEBVIEW_API void webview_navigate(webview_t w, const char *url) {
  static_cast<webview::webview *>(w)->navigate(url);
}

WEBVIEW_API void webview_init(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->init(js);
}

WEBVIEW_API void webview_eval(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->eval(js);
}

WEBVIEW_API void webview_bind(webview_t w, const char *name,
                              void (*fn)(const char *seq, const char *req,
                                         void *arg),
                              void *arg) {
  static_cast<webview::webview *>(w)->bind(
      name,
      [=](std::string seq, std::string req, void *arg) {
        fn(seq.c_str(), req.c_str(), arg);
      },
      arg);
}

WEBVIEW_API void webview_return(webview_t w, const char *seq, int status,
                                const char *result) {
  static_cast<webview::webview *>(w)->resolve(seq, status, result);
}

#endif /* WEBVIEW_HEADER */

#endif /* WEBVIEW_H */
