#pragma once

#include <string>

// Parses an XHTML chapter and extracts its readable text.
//
// - Skips <script>, <style>, <nav>, <head> and <svg> content.
// - Inserts '\n' around block-level elements (p, div, h1-h6, li, br, ...).
// - Collapses runs of whitespace and limits consecutive blank lines.
//
// Returns false if the XHTML could not be parsed; outText is empty then.
bool xmlExtractText(const std::string &xhtml, std::string &outText);
