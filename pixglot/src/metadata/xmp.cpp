#include "pixglot/details/decoder.hpp"
#include "pixglot/metadata.hpp"

#include <string_view>

#include <rapidxml.h>

using namespace rapidxml;
using namespace std::string_view_literals;

using namespace pixglot;





namespace {
  [[nodiscard]] bool is_resource(xml_node<>* node) {
    for (auto* attr = node->first_attribute(); attr != nullptr;
         attr = attr->next_attribute()) {

      if (attr->name() == "rdf:parseType"sv && attr->value() == "Resource"sv) {
        return true;
      }
    }
    return false;
  }





  [[nodiscard]] bool is_container(std::string_view name) {
    return name.size() == 7 && name.starts_with("rdf:") &&
      (name.ends_with("Seq") || name.ends_with("Alt") || name.ends_with("Bag"));
  }





  [[nodiscard]] std::string format_list(std::span<const std::string> str) {
    if (str.size() == 1) {
      return str.front();
    }

    std::string output{"["};

    for (size_t i = 0; i < str.size(); ++i) {
      if (i != 0) {
        output += ", ";
      }
      output += str[i];
    }

    output.push_back(']');
    return output;
  }




  [[nodiscard]] std::string format_value(xml_node<>* field) {
    std::string output;
    for (auto* opt = field->first_node(); opt != nullptr; opt = opt->next_sibling()) {
      if (is_container(opt->name())) {
        std::vector<std::string> list;

        for (auto* it = opt->first_node(); it != nullptr; it = it->next_sibling()) {
          if (it->name() != "rdf:li"sv) {
            continue;
          }

          if (is_resource(it)) {
            list.emplace_back("<resource>");
            continue;
          }

          list.emplace_back(it->value());
        }

        if (!output.empty()) {
          output += ", ";
        }

        output += format_list(list);
      }
    }

    if (output.empty()) {
      return field->value();
    }

    return output;
  }





  void fill_rdf_description(xml_node<>* desc, std::vector<metadata::key_value>& md) {
    for (auto* field = desc->first_node(); field != nullptr;
         field = field->next_sibling()) {


      if (is_resource(field)) {
        fill_rdf_description(field, md);
        continue;
      }


      md.emplace_back(field->name(), format_value(field));
    }
  }
}





bool fill_xmp_metadata(char* str, details::decoder& dec) {
  try {
    xml_document<> doc;
    doc.parse<0>(str);


    auto* root = doc.first_node();
    if (root == nullptr || root->name() != "x:xmpmeta"sv) {
      return false;
    }

    std::vector<metadata::key_value> md;

    for (auto* node = root->first_node(); node != nullptr; node = node->next_sibling()) {

      if (node->name() != "rdf:RDF"sv) {
        continue;
      }

      for (auto* desc = node->first_node(); desc != nullptr;
           desc = desc->next_sibling()) {

        if (desc->name() == "rdf:Description"sv) {
          fill_rdf_description(desc, md);
        }
      }
    }

    dec.image().metadata().append_move(md);

    return true;


  } catch (std::exception& error) {
    dec.warn(std::string{"unable to parse xmp: "} + error.what());
  } catch (...) {
    dec.warn("unable to parse xmp: unknown error");
  }

  return false;
}
