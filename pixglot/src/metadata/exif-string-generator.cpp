#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>



[[nodiscard]] std::pair<std::string_view, std::string_view> split(std::string_view in) {
  auto pos = in.find(' ');
  if (pos == std::string_view::npos) {
    throw std::runtime_error{"invalid tag description"};
  }

  auto name = in.substr(pos + 1);
  while (!name.empty() && (isspace(name.front()) != 0)) {
    name.remove_prefix(1);
  }
  while (!name.empty() && (isspace(name.back()) != 0)) {
    name.remove_suffix(1);
  }

  return {in.substr(0, pos), name};
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
  out << "#include <string_view>\n";

  out << "namespace pixglot::details {\n";
  out << "  std::optional<std::string_view> exif_tag_name(uint16_t tag) {\n";
  out << "    switch (tag) {\n";

  for (std::string line; std::getline(in, line); ) {
    if (line.starts_with('#') || line.empty()) {
      continue;
    }
    auto [num, str] = split(line);
    out << "      case 0x" << num << ": return \"" << str << "\";\n";
  }

  out << "    }\n";
  out << "    return {};\n";
  out << "  }\n";
  out << "}\n";
}
