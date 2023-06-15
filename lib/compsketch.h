#include<bits/stdc++.h>

// Initialization and config.
#define CS_INIT(...)                                      \
  const compsketch::RootScope compsketch_root(                    \
      compsketch::OpenFile(__LINE__, __FILE__, __VA_ARGS__),  \
      false)
#define CS_DARK(...)                                      \
  const compsketch::RootScope compsketch_root(                    \
      compsketch::OpenFile(__LINE__, __FILE__, __VA_ARGS__),  \
      true)
#define CS_SET_PRECISION(precision) compsketch::SetPrecision(precision)
#define CS_LOG_TO_STDERR(log_to_stderr) compsketch::SetLogToStderr(log_to_stderr)

// Flow operations.
#define CS_LAYER()                                  \
  compsketch::Layer compsketch_layer_##__LINE__(__LINE__);  \
  compsketch_layer_##__LINE__
#define CS_PAUSE() compsketch::Breakpoint(__LINE__)

// Draw operations.
// Args: x, y, (attr optional)
#define CS_POINT(...) compsketch::Point(__LINE__, __VA_ARGS__)
// Args: x1, y1, x2, y2, (attr optional)
#define CS_SEGMENT(...) compsketch::Segment(__LINE__, __VA_ARGS__)
// Args: x1, y1, x2, y2, x3, y3, (attr optional)
#define CS_TRIANGLE(...) compsketch::Triangle(__LINE__, __VA_ARGS__)
// Args: x1, y1, x2, y2, (attr optional)
#define CS_RECTANGLE(...) compsketch::Rectangle(__LINE__, __VA_ARGS__)
#define CS_RECT(...) compsketch::Rectangle(__LINE__, __VA_ARGS__)
// Args: x1, y1, x2, y2, (attr optional)
#define CS_LINE(...) compsketch::Line(__LINE__, __VA_ARGS__)
// Args: x, y, r, (attr optional)
#define CS_CIRCLE(...) compsketch::Circle(__LINE__, __VA_ARGS__)
#define CS_CIRC(...) compsketch::Circle(__LINE__, __VA_ARGS__)
// Args: x, y, r, sAngle, eAngle, (attr optional)
#define CS_ARC(...) compsketch::Arc(__LINE__, __VA_ARGS__)
#define CS_PIE(...) compsketch::Pie(__LINE__, __VA_ARGS__)

// Polygons and polylines.
#define CS_POLYGON(attr, code)                                \
  { compsketch::PolyBuilder poly(attr, true, __LINE__); code } 0
#define CS_POLYLINE(attr, code)                               \
  { compsketch::PolyBuilder poly(attr, false, __LINE__); code } 0
#define CS_POLYPOINT(x, y) poly.Add(x, y, __LINE__)

namespace compsketch {
// Global vars.
FILE* compsketch_file = nullptr;
std::string pad;
const std::string kDefaultResourcePath =
    "https://lukakalinovcic.github.io/geodeb";
const std::string kEmpty;
bool log_to_stderr = true;
int precision = 6;
bool poly_active = false;
bool omit_comma = true;

void AddPadding() {
  pad += "  ";
}
void RemovePadding() {
  pad.resize(pad.size() - 2);
}

void json_println(const char* format, ...) {
  if (compsketch::omit_comma) {
    fprintf(compsketch_file, "\n");
    compsketch::omit_comma = false;
  } else {
    fprintf(compsketch_file, ",\n");
  }
  fprintf(compsketch_file, "%s", pad.c_str());
  va_list args;
  va_start(args, format);
  vfprintf(compsketch_file, format, args);
  va_end(args);
}
void json_print_string(const std::string& key, const std::string& val) {
  json_println("\"%s\": \"%s\"", key.c_str(), val.c_str());
}
void json_print_int(const std::string& key, int val) {
  json_println("\"%s\": %d", key.c_str(), val);
}
void json_print_double(const std::string& key, double val) {
  static char format[64];
  sprintf(format, "\"%%s\": %%.%df", compsketch::precision);
  json_println(format, key.c_str(), val);
}
void json_start_array(const std::string& key) {
  json_println("\"%s\": [", key.c_str());
  compsketch::omit_comma = true;
  AddPadding();
}
void json_end_array() {
  RemovePadding();
  compsketch::omit_comma = true;
  json_println("]");
}
void json_start_object() {
  json_println("{");
  compsketch::omit_comma = true;
  AddPadding();
}
void json_start_object(const std::string& key) {
  json_println("\"%s\": {", key.c_str());
  compsketch::omit_comma = true;
  AddPadding();
}
void json_end_object() {
  RemovePadding();
  compsketch::omit_comma = true;
  json_println("}");
}
void log_prefix(int line) {
  const int start = std::min(8, (int)pad.size());
  fprintf(stderr, "L%03d: %s", line, pad.c_str() + start);
}
 
std::string JsonEscape(const std::string &s) {
  std::ostringstream o;
  for (auto c = s.cbegin(); c != s.cend(); c++) {
    if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
      o << "\\u"
        << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
    } else {
      o << *c;
    }
  }
  return o.str();
}

class Operation : public std::ostringstream {
 public:
  Operation(const std::string& type, int line) : line_(line) {
    json_start_object();
    json_print_string("type", type);
    json_print_int("line", line);
  }
  ~Operation() {
    if (!this->str().empty()) {
      json_print_string("label", JsonEscape(this->str()).c_str());
    }
    json_end_object();
  }
 protected:
  int line_;
};

class OperationWithLogging : public Operation {
 public:
  OperationWithLogging(const std::string& type, int line,
                       const std::vector<double>& args,
                       bool multiline=false)
      : Operation(type, line),
        multiline_(multiline) {
    if (poly_active) {
      fprintf(stderr, "compsketch: You can only use CS_POLYPOINT while building a polyline.");
      abort();
    }
    if (compsketch::log_to_stderr) {
      log_prefix(line_);
      static char format[64];
      sprintf(format, "%%.%df", compsketch::precision);
      fprintf(stderr, "%s(", type.c_str());
      for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) fprintf(stderr, ", ");
        fprintf(stderr, format, args[i]);
      }
      if (!multiline_) {
        fprintf(stderr, ")");
      } else {
        fprintf(stderr, "\n");
      }
    }
  }
  ~OperationWithLogging() {
    if (compsketch::log_to_stderr) {
      if (multiline_) {
        log_prefix(line_);
        fprintf(stderr, ")");
      }
      if (!this->str().empty()) {
        fprintf(stderr, " << \"%s\"\n", this->str().c_str());
      } else {
        fprintf(stderr, "\n");
      }
    }
  }
 private:
  bool multiline_;
};

class Layer : public OperationWithLogging {
 public:
  Layer(int line) : OperationWithLogging("layer", line, {}, true) {
    json_start_array("children");
  }
  ~Layer() {
    json_end_array();
  }
};

class Breakpoint : public OperationWithLogging {
 public:
  Breakpoint(int line) : OperationWithLogging("break", line, {}) {}
};

class Point : public OperationWithLogging {
 public:
  template<typename T>
  Point(int line, const T& p1, const std::string& attr=kEmpty) :
      Point(line, p1.x, p1.y, attr) {}
  Point(int line, double x, double y, const std::string& attr=kEmpty) :
      OperationWithLogging("point", line, {x, y}) {
    json_print_double("x", x);
    json_print_double("y", y);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Segment : public OperationWithLogging {
 public:
  template<typename T>
  Segment(int line, const T& p1, const T& p2, const std::string& attr=kEmpty) :
      Segment(line, p1.x, p1.y, p2.x, p2.y, attr) {}
  Segment(int line,
          double x1, double y1,
          double x2, double y2,
          const std::string& attr=kEmpty)
      : OperationWithLogging("segment", line, {x1, y1, x2, y2}) {
    json_print_double("x1", x1);
    json_print_double("y1", y1);
    json_print_double("x2", x2);
    json_print_double("y2", y2);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Triangle : public OperationWithLogging {
 public:
  template<typename T>
  Triangle(int line, const T& p1, const T& p2, const T& p3, const std::string& attr=kEmpty) :
      Triangle(line, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, attr) {}
  Triangle(int line,
           double x1, double y1,
           double x2, double y2,
           double x3, double y3,
           const std::string& attr=kEmpty)
      : OperationWithLogging("triangle", line, {x1, y1, x2, y2, x3, y3}) {
    json_print_double("x1", x1);
    json_print_double("y1", y1);
    json_print_double("x2", x2);
    json_print_double("y2", y2);
    json_print_double("x3", x3);
    json_print_double("y3", y3);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Rectangle : public OperationWithLogging {
 public:
  template<typename T>
  Rectangle(int line, const T& p1, const T& p2, const std::string& attr=kEmpty) :
      Rectangle(line, p1.x, p1.y, p2.x, p2.y, attr) {}
  Rectangle(int line,
            double x1, double y1,
            double x2, double y2,
            const std::string& attr=kEmpty)
      : OperationWithLogging("rect", line, {x1, y1, x2, y2}) {
    json_print_double("x1", x1);
    json_print_double("y1", y1);
    json_print_double("x2", x2);
    json_print_double("y2", y2);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Circle : public OperationWithLogging {
 public:
  template<typename T>
  Circle(int line, const T& p1, double r, const std::string& attr=kEmpty) :
      Circle(line, p1.x, p1.y, r, attr) {}
  Circle(int line, double x, double y, double r,
         const std::string& attr=kEmpty)
      : OperationWithLogging("circ", line, {x, y, r}) {
    json_print_double("x", x);
    json_print_double("y", y);
    json_print_double("r", r);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Arc : public OperationWithLogging {
 public:
  template<typename T>
  Arc(int line, const T& p1, double r, double sAngle, double eAngle,
      const std::string& attr=kEmpty) :
      Arc(line, p1.x, p1.y, r, sAngle, eAngle, attr) {}
  Arc(int line, double x, double y, double r, double sAngle, double eAngle,
      const std::string& attr=kEmpty)
      : OperationWithLogging("arc", line, {x, y, r, sAngle, eAngle}) {
    json_print_double("x", x);
    json_print_double("y", y);
    json_print_double("r", r);
    json_print_double("sAngle", sAngle);
    json_print_double("eAngle", eAngle);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Pie : public OperationWithLogging {
 public:
  template<typename T>
  Pie(int line, const T& p1, double r, double sAngle, double eAngle,
      const std::string& attr=kEmpty) :
      Pie(line, p1.x, p1.y, r, sAngle, eAngle, attr) {}
  Pie(int line, double x, double y, double r, double sAngle, double eAngle,
      const std::string& attr=kEmpty)
      : OperationWithLogging("pie", line, {x, y, r, sAngle, eAngle}) {
    json_print_double("x", x);
    json_print_double("y", y);
    json_print_double("r", r);
    json_print_double("sAngle", sAngle);
    json_print_double("eAngle", eAngle);
    if (!attr.empty()) {
      json_print_string("attr", attr);
    }
  }
};

class Line : public OperationWithLogging {
  public:
    template<typename T>
    Line(int line, const T& p1, const T& p2, const std::string& attr=kEmpty) :
        Line(line, p1.x, p1.y, p2.x, p2.y, attr) {}
    Line(int line, double x1, double y1, double x2, double y2,
        const std::string& attr=kEmpty)
        : OperationWithLogging("line", line, {x1, y1, x2, y2}) {
    	json_print_double("x1", x1);
    	json_print_double("y1", y1);
    	json_print_double("x2", x2);
    	json_print_double("y2", y2);
    	if (!attr.empty()) {
        	json_print_string("attr", attr);
    	}
    }
};

class PolyBuilder : public OperationWithLogging {
    public:
    	PolyBuilder(const std::string& attr, bool is_polygon, int line)
    : OperationWithLogging(is_polygon ? "polygon" : "polyline", line, {}, true) {
    	compsketch::poly_active = true;
    	if (!attr.empty()) {
    		json_print_string("attr", attr);
    	}
    	json_start_array("points");
    }
    ~PolyBuilder() {
    	json_end_array();
    	compsketch::poly_active = false;
    }

    void Add(double x, double y, int line) {
    	static char format[64];
    	sprintf(format, "[%%.%df, %%.%df]",
        	    compsketch::precision, compsketch::precision);
    	json_println(format, x, y);
    	if (compsketch::log_to_stderr) {
        	sprintf(format, "(%%.%df, %%.%df)\n",
                compsketch::precision, compsketch::precision);
        	log_prefix(line);
        	fprintf(stderr, format, x, y);
        }
    }
};

class RootScope {
    public:
    	RootScope(FILE* f, bool dark) : f(f), dark_(dark) {}
    ~RootScope() {
    	json_end_array();
    	json_end_object();
    	json_print_string("theme", dark_ ? "dark" : "light");
    	json_end_object();
    	fprintf(compsketch_file, ";\n");
    	fprintf(compsketch_file, "init(resourcePath);\n");
    	fprintf(compsketch_file, "</script>\n");
    	fprintf(compsketch_file, "</html>\n");
    	fclose(f);
    	fprintf(stderr, "compsketch: Don't forget to remove the debug code before "
            "submitting to online judges!\n");
    }
    private:
  		FILE *f;
  		bool dark_;
};

FILE* OpenFile(int line,
               const std::string& source_filename,
               const std::string& output_filename,
               const std::string& resource_path=kDefaultResourcePath) {
    assert(compsketch_file == nullptr);
    compsketch_file = fopen(output_filename.c_str(), "wt");
    assert(compsketch_file != nullptr);

    fprintf(compsketch_file, "<!DOCTYPE html>\n");
    fprintf(compsketch_file, "<html>\n");
    fprintf(compsketch_file, "<head>\n");
    fprintf(compsketch_file, "  <script src=\"%s/main.js\"></script>\n", resource_path.c_str());
    fprintf(compsketch_file, "</head>\n");
    fprintf(compsketch_file, "<body>\n");
    fprintf(compsketch_file, "  <div id=\"rootElement\"></div>\n");
    fprintf(compsketch_file, "</body>\n");
    fprintf(compsketch_file, "<script type=\"text/javascript\">\n");
    fprintf(compsketch_file, "resourcePath = '%s/';\n", resource_path.c_str());
    fprintf(compsketch_file, "jsonData = ");
    json_start_object();
    FILE* source_file = fopen(source_filename.c_str(), "rt");
    if (source_file != nullptr) {
    	json_start_array("source_code");
    	static char buffer[2048];
    	while (fgets(buffer, 2048, source_file) != nullptr) {
        	json_println("\"%s\"", JsonEscape(buffer).c_str());
    	}
    	fclose(source_file);
    	json_end_array();
    }
    json_start_object("root");
    json_print_string("type", "begin");
    json_print_int("line", line);
    json_start_array("children");
    return compsketch_file;
}

void SetPrecision(int precision) {
    assert(precision >= 0 && precision <= 20);
    compsketch::precision = precision;
}

void SetLogToStderr(bool log_to_stderr) {
    compsketch::log_to_stderr = log_to_stderr;
}

}  // namespace compsketch
