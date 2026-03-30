#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include "Vector2D.h"

namespace engine {
namespace core {

/// Serializer — Lectura/escritura JSON ligero sin dependencias externas.
///
/// Compatible con -fno-exceptions. Escribe y lee un subconjunto de JSON
/// suficiente para serializar escenas del ECS.
///
/// No es un parser JSON completo — es un writer/reader minimalista
/// diseñado para serialización de componentes.
///

// ── JSON Writer ────────────────────────────────────────────────

class JsonWriter {
public:
    void beginObject() {
        comma();
        m_out += '{';
        m_depth++;
        m_needComma = false;
    }

    void endObject() {
        m_depth--;
        newline();
        m_out += '}';
        m_needComma = true;
    }

    void beginArray(const std::string& key = "") {
        comma();
        if (!key.empty()) { m_out += '"'; m_out += key; m_out += "\": "; }
        m_out += '[';
        m_depth++;
        m_needComma = false;
    }

    void endArray() {
        m_depth--;
        newline();
        m_out += ']';
        m_needComma = true;
    }

    void key(const std::string& k) {
        comma();
        m_out += '"';
        m_out += k;
        m_out += "\": ";
        m_needComma = false;
    }

    void value(float v) {
        comma();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", v);
        m_out += buf;
        m_needComma = true;
    }

    void value(int v) {
        comma();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", v);
        m_out += buf;
        m_needComma = true;
    }

    void value(bool v) {
        comma();
        m_out += (v ? "true" : "false");
        m_needComma = true;
    }

    void value(const std::string& v) {
        comma();
        m_out += '"';
        m_out += v;
        m_out += '"';
        m_needComma = true;
    }

    void keyValue(const std::string& k, float v) { key(k); value(v); }
    void keyValue(const std::string& k, int v) { key(k); value(v); }
    void keyValue(const std::string& k, bool v) { key(k); value(v); }
    void keyValue(const std::string& k, const std::string& v) { key(k); value(v); }

    void keyValue(const std::string& k, const math::Vector2D& v) {
        key(k);
        beginObject();
        keyValue("x", v.x);
        keyValue("y", v.y);
        endObject();
    }

    const std::string& toString() const { return m_out; }

    bool saveToFile(const std::string& path) {
        std::FILE* file = std::fopen(path.c_str(), "w");
        if (!file) return false;
        size_t written = std::fwrite(m_out.data(), 1, m_out.size(), file);
        std::fclose(file);
        return written == m_out.size();
    }

private:
    void comma() {
        if (m_needComma) m_out += ", ";
    }

    void newline() {
        m_out += '\n';
        for (int i = 0; i < m_depth; i++) m_out += "  ";
    }

    std::string m_out;
    int  m_depth     = 0;
    bool m_needComma = false;
};

// ── JSON Token Reader ──────────────────────────────────────────

/// Token-based JSON reader for loading scenes.
/// Reads character by character, no exceptions needed.
class JsonReader {
public:
    bool loadFromFile(const std::string& path) {
        std::FILE* file = std::fopen(path.c_str(), "r");
        if (!file) return false;
        std::fseek(file, 0, SEEK_END);
        long sz = std::ftell(file);
        if (sz <= 0) { std::fclose(file); return false; }
        std::fseek(file, 0, SEEK_SET);
        m_data.resize(static_cast<size_t>(sz));
        size_t read = std::fread(&m_data[0], 1, static_cast<size_t>(sz), file);
        m_data.resize(read);
        std::fclose(file);
        m_pos = 0;
        return true;
    }

    bool loadFromString(const std::string& json) {
        m_data = json;
        m_pos = 0;
        return true;
    }

    // ── Navigation ─────────────────────────────────────────────

    bool expectChar(char c) {
        skipWhitespace();
        if (m_pos < m_data.size() && m_data[m_pos] == c) {
            m_pos++;
            return true;
        }
        return false;
    }

    bool peekChar(char c) {
        skipWhitespace();
        return m_pos < m_data.size() && m_data[m_pos] == c;
    }

    bool hasMore() {
        skipWhitespace();
        return m_pos < m_data.size();
    }

    // ── Value reading ──────────────────────────────────────────

    std::string readString() {
        skipWhitespace();
        if (m_pos >= m_data.size() || m_data[m_pos] != '"') return "";
        m_pos++;  // skip opening quote
        std::string result;
        while (m_pos < m_data.size() && m_data[m_pos] != '"') {
            if (m_data[m_pos] == '\\' && m_pos + 1 < m_data.size()) {
                m_pos++;
                result += m_data[m_pos];
            } else {
                result += m_data[m_pos];
            }
            m_pos++;
        }
        if (m_pos < m_data.size()) m_pos++;  // skip closing quote
        return result;
    }

    float readFloat() {
        skipWhitespace();
        size_t start = m_pos;
        while (m_pos < m_data.size() &&
               (m_data[m_pos] == '-' || m_data[m_pos] == '.' ||
                (m_data[m_pos] >= '0' && m_data[m_pos] <= '9') ||
                m_data[m_pos] == 'e' || m_data[m_pos] == 'E' || m_data[m_pos] == '+')) {
            m_pos++;
        }
        if (m_pos == start) return 0.0f;
        // strtof en vez de stof — seguro con -fno-exceptions
        const char* begin = m_data.c_str() + start;
        char* endp = nullptr;
        float val = std::strtof(begin, &endp);
        return (endp == begin) ? 0.0f : val;
    }

    int readInt() {
        return static_cast<int>(readFloat());
    }

    bool readBool() {
        skipWhitespace();
        if (m_pos + 4 <= m_data.size() && m_data.compare(m_pos, 4, "true") == 0) {
            m_pos += 4;
            return true;
        }
        if (m_pos + 5 <= m_data.size() && m_data.compare(m_pos, 5, "false") == 0) {
            m_pos += 5;
            return false;
        }
        return false;
    }

    math::Vector2D readVector2D() {
        math::Vector2D v;
        if (!expectChar('{')) return v;
        while (!peekChar('}')) {
            std::string k = readKey();
            if (k == "x") v.x = readFloat();
            else if (k == "y") v.y = readFloat();
            skipComma();
        }
        expectChar('}');
        return v;
    }

    std::string readKey() {
        std::string k = readString();
        expectChar(':');
        return k;
    }

    void skipComma() {
        skipWhitespace();
        if (m_pos < m_data.size() && m_data[m_pos] == ',') m_pos++;
    }

    void skipValue() {
        skipWhitespace();
        if (m_pos >= m_data.size()) return;
        char c = m_data[m_pos];
        if (c == '"') { readString(); }
        else if (c == '{') { skipObject(); }
        else if (c == '[') { skipArray(); }
        else if (c == 't' || c == 'f') { readBool(); }
        else { readFloat(); }
    }

private:
    void skipWhitespace() {
        while (m_pos < m_data.size() &&
               (m_data[m_pos] == ' ' || m_data[m_pos] == '\n' ||
                m_data[m_pos] == '\r' || m_data[m_pos] == '\t')) {
            m_pos++;
        }
    }

    void skipObject() {
        expectChar('{');
        int depth = 1;
        while (m_pos < m_data.size() && depth > 0) {
            if (m_data[m_pos] == '{') depth++;
            if (m_data[m_pos] == '}') depth--;
            if (m_data[m_pos] == '"') { readString(); continue; }
            m_pos++;
        }
    }

    void skipArray() {
        expectChar('[');
        int depth = 1;
        while (m_pos < m_data.size() && depth > 0) {
            if (m_data[m_pos] == '[') depth++;
            if (m_data[m_pos] == ']') depth--;
            if (m_data[m_pos] == '"') { readString(); continue; }
            m_pos++;
        }
    }

    std::string m_data;
    size_t m_pos = 0;
};

} // namespace core
} // namespace engine
