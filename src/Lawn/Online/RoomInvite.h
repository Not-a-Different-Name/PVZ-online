#pragma once
#include "RelayNodes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace RoomInvite {
constexpr std::size_t MaxText = 4096;
struct Intent { std::string code, node; bool Routed() const { return !node.empty(); } };
inline char Upper(char value) { return value >= 'a' && value <= 'z' ? char(value - 'a' + 'A') : value; }
inline bool Code(std::string value, std::string& output) {
    if (value.size() != 4) return false;
    for (char& letter : value) {
        letter = Upper(letter);
        if (std::string("ABCDEFGHJKLMNPQRSTUVWXYZ23456789").find(letter) == std::string::npos) return false;
    }
    output = value; return true;
}
inline bool Node(const std::string& value) {
    if (value.empty() || value.size() > 5 || value.front() == '0') return false;
    unsigned number = 0;
    for (char digit : value) { if (digit < '0' || digit > '9') return false; number = number * 10 + unsigned(digit - '0'); }
    return number && number <= 65535;
}
inline std::string Encode(const std::string& value) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string output;
    unsigned accumulator = 0; int bits = 0;
    for (unsigned char byte : value) {
        accumulator = (accumulator << 8) | byte; bits += 8;
        while (bits >= 6) { bits -= 6; output += alphabet[(accumulator >> bits) & 63]; }
    }
    if (bits) output += alphabet[(accumulator << (6 - bits)) & 63];
    return output;
}
inline bool Decode(const std::string& value, std::string& output) {
    if (value.empty() || value.size() > 32) return false;
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string result; unsigned accumulator = 0; int bits = 0;
    for (char letter : value) {
        const auto index = alphabet.find(letter); if (index == std::string::npos) return false;
        accumulator = (accumulator << 6) | unsigned(index); bits += 6;
        if (bits >= 8) { bits -= 8; result += char((accumulator >> bits) & 255); }
    }
    if (Encode(result) != value) return false;
    output = result; return true;
}
inline bool Token(const std::string& value, Intent& output) {
    Intent next;
    const auto dash = value.find('-');
    if (dash == std::string::npos) { if (!Code(value, next.code)) return false; }
    else {
        next.node = value.substr(0, dash);
        if (!Node(next.node) || !Code(value.substr(dash + 1), next.code)) return false;
    }
    output = next; return true;
}
inline bool Merge(Intent& result, const Intent& next) {
    if (!result.code.empty() && (result.code != next.code ||
        (!result.node.empty() && !next.node.empty() && result.node != next.node))) return false;
    result.code = next.code;
    if (!next.node.empty()) result.node = next.node;
    return true;
}
inline bool Parse(std::string text, Intent& output) {
    if (text.empty() || text.size() > MaxText) return false;
    if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if ((!byte || byte < 32) && byte != '\r' && byte != '\n' && byte != '\t') return false;
        if (text.compare(index, 3, "\xe3\x80\x80") == 0) text.replace(index, 3, " ");
        if (text.compare(index, 2, "\xc2\xa0") == 0) text.replace(index, 2, " ");
    }
    std::string compact;
    for (char letter : text) if (letter != ' ' && letter != '\t' && letter != '\r' && letter != '\n') compact += letter;
    Intent result;
    if (Token(compact, result)) { output = result; return true; }
    const std::string marker = "pvzroom:";
    std::string lower=text;for(char& letter:lower)if(letter>='A'&&letter<='Z')letter=char(letter-'A'+'a');
    const auto mark = lower.find(marker);
    if (mark != std::string::npos) {
        if (lower.find(marker, mark + marker.size()) != std::string::npos) return false;
        const auto end = text.find_first_of(" \t\r\n", mark + marker.size());
        const auto encoded = text.substr(mark + marker.size(), end == std::string::npos ? end : end - mark - marker.size());
        std::string decoded;
        if (!Decode(encoded, decoded) || !Token(decoded, result) || !result.Routed()) return false;
        text.erase(mark, end == std::string::npos ? end : end - mark);
    }
    if (text.find("://") != std::string::npos || text.find('/') != std::string::npos) return false;
    for (std::size_t begin = 0; begin < text.size();) {
        auto part = [](unsigned char letter) { return (letter >= 'A' && letter <= 'Z') ||
            (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '-'; };
        while (begin < text.size() && !part(static_cast<unsigned char>(text[begin]))) ++begin;
        auto end = begin; while (end < text.size() && part(static_cast<unsigned char>(text[end]))) ++end;
        if (end == begin) break;
        Intent next;
        const auto token=text.substr(begin,end-begin);const auto dash=token.find('-');std::string tail;
        if(dash!=std::string::npos&&Code(token.substr(dash+1),tail)&&!Node(token.substr(0,dash)))return false;
        if (Token(text.substr(begin, end - begin), next) && !Merge(result, next)) return false;
        begin = end;
    }
    if (result.code.empty()) return false;
    output = result; return true;
}
inline std::string Share(const RelayNodes::Endpoint& endpoint, const std::string& code) {
    std::string normalized;
    const auto* node = RelayNodes::Find(endpoint);
    if (!node || !Code(code, normalized)) return {};
    return "PVZ 房间 " + normalized + "\npvzroom:" + Encode(node->id + "-" + normalized);
}
}
