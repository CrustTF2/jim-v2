// Resolve a TF2 paint definition to its ordered compositor graph.
#pragma once
#include "paint_definitions.hpp"
#include <google/protobuf/descriptor.h>
#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

struct PaintStage {
    std::string kind;
    std::vector<std::pair<std::string,std::string>> fields;
    std::vector<PaintStage> children;
};
class PaintGraph {
    const PaintDefinitions& defs_;
    std::map<std::string,std::string> values_;
    std::string resolve(const CMsgVarField& value)const {
        if(value.has_variable()) {
            auto v=values_.find(value.variable());if(v==values_.end())throw std::runtime_error("Missing paint variable: "+value.variable());return v->second;
        }
        if(value.has_string())return value.string();
        if(value.has_float_())return std::to_string(value.float_());
        if(value.has_double_())return std::to_string(value.double_());
        if(value.has_uint32())return std::to_string(value.uint32());
        if(value.has_uint64())return std::to_string(value.uint64());
        if(value.has_sint32())return std::to_string(value.sint32());
        if(value.has_sint64())return std::to_string(value.sint64());
        if(value.has_bool_())return value.bool_()?"True":"False";
        return "";
    }
    static std::string lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
    void convert(const CMsgPaintKit_OperationNode& node,std::vector<PaintStage>& output,std::set<uint32_t> ancestors)const {
        if(node.has_operation_template()) {
            auto id=node.operation_template().defindex();if(!ancestors.insert(id).second)throw std::runtime_error("Cyclic paint operation");
            auto it=defs_.operations.find(id);if(it==defs_.operations.end())throw std::runtime_error("Paint operation missing");
            for(const auto& next:it->second.operation_node())convert(next,output,ancestors);return;
        }
        if(!node.has_stage())throw std::runtime_error("Invalid paint operation node");
        auto& stage=node.stage();const auto* descriptor=stage.GetDescriptor();const auto* reflection=stage.GetReflection();
        const auto* active=reflection->GetOneofFieldDescriptor(stage,descriptor->oneof_decl(0));
        if(!active)throw std::runtime_error("Empty paint operation");
        const auto& message=reflection->GetMessage(stage,active);const auto* ref=message.GetReflection();
        std::vector<const google::protobuf::FieldDescriptor*> fields;ref->ListFields(message,&fields);PaintStage current;current.kind=active->name();
        for(const auto* field:fields) {
            int count=field->is_repeated()?ref->FieldSize(message,field):1;
            for(int i=0;i<count;++i) {
                const auto& v=field->is_repeated()?ref->GetRepeatedMessage(message,field,i):ref->GetMessage(message,field);
                if(field->name()=="operation_node")convert(dynamic_cast<const CMsgPaintKit_OperationNode&>(v),current.children,ancestors);
                else if(field->name()=="sticker") {
                    const auto& sticker=dynamic_cast<const CMsgPaintKit_Operation_Sticker&>(v);
                    current.fields.emplace_back("sticker_base",sticker.has_base()?resolve(sticker.base()):"");
                    current.fields.emplace_back("sticker_weight",sticker.has_weight()?resolve(sticker.weight()):"1");
                    current.fields.emplace_back("sticker_spec",sticker.has_spec()?resolve(sticker.spec()):"");
                } else current.fields.emplace_back(field->name(),resolve(dynamic_cast<const CMsgVarField&>(v)));
            }
        }
        output.emplace_back(std::move(current));
    }
public:
    PaintGraph(const PaintDefinitions& defs,uint32_t id,const std::string& model,float wear,const std::map<uint32_t,std::string>& catalog):defs_(defs) {
        auto found=defs.paints.find(id);if(found==defs.paints.end())throw std::runtime_error("Paint not found");
        const auto& paint=found->second;const auto* ref=paint.GetReflection();std::vector<const google::protobuf::FieldDescriptor*> fields;
        ref->ListFields(paint,&fields);const CMsgPaintKit_Definition_Item* choice=nullptr;const CMsgPaintKit_ItemDefinition* spec=nullptr;
        for(const auto* field:fields) {
            if(field->cpp_type()!=google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE || field->message_type()!=CMsgPaintKit_Definition_Item::descriptor())continue;
            int count=field->is_repeated()?ref->FieldSize(paint,field):1;
            for(int i=0;i<count;++i) {
                const auto& entry=dynamic_cast<const CMsgPaintKit_Definition_Item&>(field->is_repeated()?ref->GetRepeatedMessage(paint,field,i):ref->GetMessage(paint,field));
                auto it=defs.items.find(entry.item_definition_template().defindex());if(it==defs.items.end())continue;
                auto cat=catalog.find(it->second.item_definition_index());
                if(cat!=catalog.end() && lower(cat->second)==lower(model)){choice=&entry;spec=&it->second;break;}
            }
            if(choice)break;
        }
        if(!choice||!spec)throw std::runtime_error("Weapon not supported by War Paint");
        std::map<std::string,bool> allowed;
        for(const auto& v:paint.header().variables()){values_[v.name()]=v.value();allowed[v.name()]=v.inherit();}
        auto put=[&](const std::string& key,const std::string& value){auto flag=allowed.find(key);if(!values_.count(key)||flag==allowed.end()||flag->second)values_[key]=value;};
        for(const auto& v:choice->data().variable())if(v.has_string())put(v.variable(),v.string());
        for(const auto& v:spec->header().variables())put(v.name(),v.value());
        int level=wear<=.2f?0:wear<=.4f?1:wear<=.6f?2:wear<=.8f?3:4;
        if(wear<0||wear>1)throw std::runtime_error("Invalid paint wear");
        if(level<spec->definition_size())for(const auto& v:spec->definition(level).variable())if(v.has_string())put(v.variable(),v.string());
        auto operation=(level<spec->definition_size()&&spec->definition(level).has_operation_template())?spec->definition(level).operation_template().defindex():paint.operation_template().defindex();
        auto root=defs.operations.find(operation);if(root==defs.operations.end())throw std::runtime_error("Root paint operation missing");
        std::vector<PaintStage> result;for(const auto& node:root->second.operation_node())convert(node,result,{operation});
        if(result.size()!=1)throw std::runtime_error("Unexpected number of paint graph roots");graph=std::move(result.front());
    }
    PaintStage graph;
};
