#include "JsonTreeNode.h"
#include <algorithm>

std::string JsonTreeNode::getKey() const { return mKey; }

std::string JsonTreeNode::getValue() const { return mValue; }

json_spirit::Value_type JsonTreeNode::getType() const { return mType; }

bool JsonTreeNode::isRealJsonNode() const { return realJsonNode; }

bool JsonTreeNode::isCompoundType() const
{
    return getType() == json_spirit::obj_type || getType() == json_spirit::array_type;
}

JsonTreeNode::JsonTreeNode() {}

JsonTreeNode::~JsonTreeNode() {}

void JsonTreeNode::sortChildrenByName()
{
    std::sort(children.begin(), children.end(),
              [](const std::shared_ptr<JsonTreeNode>& obj1, const std::shared_ptr<JsonTreeNode>& obj2) {
                  return (obj1->mKey < obj2->mKey);
              });
}

void JsonTreeNode::setType(json_spirit::Value_type t) { mType = t; }

void JsonTreeNode::setKey(const std::string& k) { mKey = k; }

void JsonTreeNode::setValue(const std::string& v) { mValue = v; }

void JsonTreeNode::setIsRealJsonNode(bool val) { realJsonNode = val; }

std::shared_ptr<JsonTreeNode> JsonTreeNode::ImportFromJson(const json_spirit::Value& value,
                                                           JsonTreeNode*             parent)
{
    std::shared_ptr<JsonTreeNode> rootItem = std::make_shared<JsonTreeNode>();
    rootItem->setParentObject(parent);
    rootItem->setKey("root");
    rootItem->setType(value.type());

    if (value.type() == json_spirit::Value_type::obj_type) {

        // Get all Value childs
        for (const json_spirit::Pair& p : value.get_obj()) {
            json_spirit::Value            v     = p.value_;
            std::shared_ptr<JsonTreeNode> child = ImportFromJson(v, rootItem.get());
            child->setParentObject(rootItem.get());
            child->setKey(p.name_);
            child->setType(v.type());
            rootItem->appendChild(child);
            SetCompoundTypeValueLabel(child.get());
        }
    }

    else if (value.type() == json_spirit::Value_type::array_type) {
        // Get all Value childs
        int index = 0;
        for (json_spirit::Value v : value.get_array()) {
            std::shared_ptr<JsonTreeNode> child = ImportFromJson(v, rootItem.get());
            child->setParentObject(rootItem.get());
            child->setKey(std::to_string(index));
            child->setIsRealJsonNode(false);
            rootItem->appendChild(child);
            ++index;
            SetCompoundTypeValueLabel(child.get());
        }
    } else {
        switch (value.type()) {
        case json_spirit::Value_type::str_type:
            rootItem->setValue(value.get_str());
            break;
        case json_spirit::Value_type::int_type:
            rootItem->setValue(std::to_string(value.get_int64()));
            break;
        case json_spirit::Value_type::bool_type:
            rootItem->setValue(value.get_bool() ? "true" : "false");
            break;
        case json_spirit::Value_type::real_type: {
            double            d = value.get_real();
            std::stringstream s;
            s << d;
            rootItem->setValue(s.str());
            break;
        }
        case json_spirit::Value_type::null_type:
            break;
        default:
            throw std::runtime_error("Unhandled json_spirit value type with " +
                                     std::to_string(value.type()));
        }
    }

    return rootItem;
}

json_spirit::Value JsonTreeNode::ExportToJson(const JsonTreeNode* root)
{
    if (!root) {
        return json_spirit::Value();
    }
    json_spirit::Value res;
    switch (root->getType()) {
    case json_spirit::Value_type::array_type:
        res = json_spirit::Array();
        for (unsigned i = 0; i < root->countChildren(); i++) {
            JsonTreeNode*      ni = (*root)[i].get();
            json_spirit::Value t  = ExportToJson(ni);
            res.get_array().push_back(t);
        }
        break;
    case json_spirit::Value_type::obj_type:
        res = json_spirit::Object();
        for (unsigned i = 0; i < root->countChildren(); i++) {
            JsonTreeNode*      ni = (*root)[i].get();
            json_spirit::Value t  = ExportToJson(ni);
            res.get_obj().push_back(json_spirit::Pair(ni->getKey(), t));
        }
        break;
    case json_spirit::Value_type::str_type:
        res = json_spirit::Value(std::string(root->getValue()));
        break;
    case json_spirit::Value_type::int_type: {
        std::string vs = root->getValue();
        int64_t     v  = std::stoll(vs);
        res            = json_spirit::Value(v);
        break;
    }
    case json_spirit::Value_type::bool_type: {
        std::string bVal = root->getValue();
        std::transform(bVal.begin(), bVal.end(), bVal.begin(), ::tolower);
        bool v = (bVal == "true" ? true : false);
        res    = json_spirit::Value(v);
        break;
    }
    case json_spirit::Value_type::real_type: {
        std::stringstream ss(root->getValue());
        double            v;
        ss >> v;
        res = json_spirit::Value(v);
        break;
    }
    case json_spirit::Value_type::null_type: {
        break;
    }
    }
    return res;
}

std::string JsonTreeNode::NodeTypeToString(const JsonTreeNode* node)
{
    if (!node) {
        return "nullptr";
    }

    return TypeToString(node->getType());
}

std::string JsonTreeNode::TypeToString(json_spirit::Value_type t)
{
    switch (t) {
    case json_spirit::Value_type::array_type:
        return "Array";
    case json_spirit::Value_type::bool_type:
        return "Boolean";
    case json_spirit::Value_type::int_type:
        return "Integer";
    case json_spirit::Value_type::null_type:
        return "Null";
    case json_spirit::Value_type::obj_type:
        return "Object";
    case json_spirit::Value_type::real_type:
        return "Real";
    case json_spirit::Value_type::str_type:
        return "String";
    default:
        return "Unknown";
    }
}

void JsonTreeNode::SetCompoundTypeValueLabel(JsonTreeNode* node)
{
    if (node->getType() == json_spirit::obj_type) {
        node->setValue("{" + std::to_string(node->countChildren()) + "}");
    }
    if (node->getType() == json_spirit::array_type) {
        node->setValue("[" + std::to_string(node->countChildren()) + "]");
    }
}

bool JsonTreeNode::TestValueStorage(const std::string& valStr, json_spirit::Value_type type)
{
    if (valStr.empty()) {
        return false;
    }

    switch (type) {
    case json_spirit::Value_type::array_type:
        return false;
    case json_spirit::Value_type::bool_type:
        return valStr == "true" || valStr == "false";
        break;
    case json_spirit::Value_type::int_type:
        try {
            std::stoll(valStr);
            return true;
        } catch (...) {
            return false;
        }
    case json_spirit::Value_type::null_type:
        return false;
    case json_spirit::Value_type::obj_type:
        return false;
    case json_spirit::Value_type::real_type:
        try {
            std::stod(valStr);
            return true;
        } catch (...) {
            return false;
        }
    case json_spirit::Value_type::str_type:
        return true;
    default:
        return false;
    }
}
