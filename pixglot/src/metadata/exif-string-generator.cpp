#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>



[[nodiscard]] std::string_view trim(std::string_view in) {
  while (!in.empty() && (isspace(in.front()) != 0)) {
    in.remove_prefix(1);
  }

  while (!in.empty() && (isspace(in.back()) != 0)) {
    in.remove_suffix(1);
  }

  return in;
}



[[nodiscard]] std::vector<std::string_view> split(std::string_view in, char delim) {
  std::vector<std::string_view> out;

  for (auto pos = in.find(delim); pos != std::string_view::npos; pos = in.find(delim)) {
    if (auto str = trim(in.substr(0, pos)); !str.empty()) {
      out.emplace_back(str);
    }
    in.remove_prefix(pos + 1);
  }

  if (auto str = trim(in); !in.empty()) {
    out.emplace_back(str);
  }
  return out;
}



int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  if (args.size() != 3) {
    std::cout << "usage: " << args[0] << " <input> <output.cpp>" << std::endl;
    return 0;
  }
  std::ofstream out{args[2]};
  std::ifstream in{args[1]};
  out << "#include <cstdint>\n";
  out << "#include <optional>\n";
  out << "#include <string>\n";
  out << "#include <string_view>\n";

  out << "//NOLINTBEGIN(*-cognitive-complexity)\n";
  out << "namespace pixglot::details {\n";
  out << "  std::optional<std::string_view> exif_tag_name(uint16_t tag) {\n";
  out << "    switch (tag) {\n";

  for (std::string line; std::getline(in, line); ) {
    if (line.starts_with('#') || line.empty()) {
      continue;
    }
    auto list = split(line, ' ');
    if (list.size() < 2) {
      throw std::runtime_error{"missing id or name: " + line};
    }
    out << "      case 0x" << list[0] << ": return \"" << list[1] << "\";\n";
  }

  out << "    }\n";
  out << "    return {};\n";
  out << "  }\n";

  in = std::ifstream{args[1]};
  out << "\n\n";
  out << "  std::string exif_transform_value(uint16_t tag, std::string input) {\n";
  out << "    switch (tag) {\n";

  for (std::string line; std::getline(in, line); ) {
    if (line.starts_with('#') || line.empty()) {
      continue;
    }
    auto list = split(line, '|');
    if (list.size() <= 1) {
      continue;
    }
    auto front = split(list[0], ' ');
    if (front.size() <= 1) {
      throw std::runtime_error{"missing tag id: " + line};
    }
    auto back = split(list[1], ';');
    if (back.empty()) {
      throw std::runtime_error{"missing key=value pairs: " + line};
    }
    out << "      case 0x" << front[0] << ":\n";
    for (auto item: back) {
      auto sep = split(item, '=');
      if (sep.size() != 2) {
        throw std::runtime_error{"wrong key=value format: " + line};
      }
      out << "        if (input == \"" << sep[0] << "\") { return \""
          << sep[1] << "\"; }\n";
    }
    out << "        break;\n";
  }

  out << "    }\n";
  out << "    return input;\n";
  out << "  }\n";
  out << "}\n";
  out << "//NOLINTEND(*-cognitive-complexity)\n";
}
