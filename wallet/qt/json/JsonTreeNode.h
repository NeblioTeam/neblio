#ifndef JSON_TREE_NODE_H
#define JSON_TREE_NODE_H

#include "AbstractTreeNode.h"
#include "json_spirit.h"

#include <string>

class JsonTreeNode : public AbstractTreeNode<JsonTreeNode, std::shared_ptr, std::enable_shared_from_this>
{
private:
    std::string             mKey;
    std::string             mValue;
    json_spirit::Value_type mType = json_spirit::null_type;
    // some times the tree has elements that are not represented in a json node; a good example of this
    // is arrays. Array indices are shown as children, but they don't represent anything in Json
    bool realJsonNode = true;

public:
    JsonTreeNode();
    virtual ~JsonTreeNode();
    void                    sortChildrenByName();
    void                    setType(json_spirit::Value_type t);
    void                    setKey(const std::string& k);
    void                    setValue(const std::string& v);
    void                    setIsRealJsonNode(bool val);
    std::string             getKey() const;
    std::string             getValue() const;
    json_spirit::Value_type getType() const;
    bool                    isRealJsonNode() const;
    bool                    isCompoundType() const;

    static std::shared_ptr<JsonTreeNode> ImportFromJson(const json_spirit::Value& getValue,
                                                        JsonTreeNode*             parent = 0);
    static json_spirit::Value            ExportToJson(const JsonTreeNode* root);

    static std::string NodeTypeToString(const JsonTreeNode* node);
    static std::string TypeToString(json_spirit::Value_type t);

    static void SetCompoundTypeValueLabel(JsonTreeNode* node);

    /// tests whether converting from string to json type is OK (used primarily for editing)
    static bool TestValueStorage(const std::string& valStr, json_spirit::Value_type type);
};

#endif // JSON_TREE_NODE_H
