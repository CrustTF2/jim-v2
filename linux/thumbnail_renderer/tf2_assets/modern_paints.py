"""Approximate newer War Paints from the installed proto definitions."""
from pathlib import Path
import struct
import numpy as np
from . import tf_proto_def_messages_pb2 as pb
from .models import AssetError
from .wear import wear_level

class ModernPaints:
    def __init__(self,tf):
        data=(Path(tf)/'scripts/protodefs/proto_defs.vpd').read_bytes()
        self.records={};pos=0
        kinds={7:pb.CMsgPaintKit_Operation,8:pb.CMsgPaintKit_ItemDefinition,9:pb.CMsgPaintKit_Definition}
        while pos<len(data):
            if pos+8>len(data):raise AssetError('Truncated proto definitions')
            kind,count=struct.unpack_from('<II',data,pos);pos+=8
            for _ in range(count):
                if pos+4>len(data):raise AssetError('Truncated proto record')
                size=struct.unpack_from('<I',data,pos)[0];pos+=4
                if pos+size>len(data):raise AssetError('Invalid proto record length')
                if kind in kinds:
                    item=kinds[kind]();item.ParseFromString(data[pos:pos+size]);self.records[kind,item.header.defindex]=item
                pos+=size

    def render(self,compositor,paint_id,model_path,catalog,wear):
        paint=self.records.get((9,paint_id))
        if paint is None:raise AssetError(f'War Paint #{paint_id} is not in the installed TF2 definitions')
        model_path=model_path.casefold()
        entries=[]
        for field,value in paint.ListFields():
            seq=value if field.label == field.LABEL_REPEATED else [value]
            for item in seq:
                if isinstance(item,pb.CMsgPaintKit_Definition.Item):entries.append(item)
        choice=None
        for entry in entries:
            spec=self.records.get((8,entry.item_definition_template.defindex))
            if spec is None:continue
            match=next((x for x in catalog['weapons'] if x['id']==spec.item_definition_index),None)
            if match and match['model'].casefold()==model_path:
                choice=(entry,spec);break
        if choice is None:raise AssetError('This weapon is not supported by that War Paint')
        entry,spec=choice
        values={v.name:v.value for v in paint.header.variables}
        allowed={v.name:v.inherit for v in paint.header.variables}
        def put(name,value):
            if name not in values or allowed.get(name,True):values[name]=value
        for v in entry.data.variable:
            if v.WhichOneof('value')=='string':put(v.variable,v.string)
        for v in spec.header.variables:put(v.name,v.value)
        level=wear_level(wear)-1
        if level<len(spec.definition):
            for v in spec.definition[level].variable:
                if v.WhichOneof('value')=='string':put(v.variable,v.string)
        wear_spec=spec.definition[level] if level<len(spec.definition) else None
        ref=wear_spec.operation_template if wear_spec and wear_spec.HasField('operation_template') else paint.operation_template
        root=self.records.get((7,ref.defindex))
        if root is None:raise AssetError(f'Paint operation #{ref.defindex} is unavailable')

        def resolve_var(var):
            if var.HasField('variable'):
                if var.variable not in values:raise AssetError('Missing paint variable: '+var.variable)
                return values[var.variable]
            actual=var.WhichOneof('value')
            return str(getattr(var,actual)) if actual else ''

        def convert_operation(operation,trail=()):
            ident=operation.header.defindex
            if ident in trail:raise AssetError('Cyclic paint operation')
            output=[]
            for node in operation.operation_node:
                which=node.WhichOneof('node')
                if which=='operation_template':
                    sub=self.records.get((7,node.operation_template.defindex))
                    if sub is None:raise AssetError(f'Missing paint operation #{node.operation_template.defindex}')
                    output.extend(convert_operation(sub,trail+(ident,)))
                elif which=='stage':
                    name=node.stage.WhichOneof('stage')
                    if not name:raise AssetError('Empty paint operation')
                    stage=getattr(node.stage,name);fields=[]
                    for field,value in stage.ListFields():
                        if field.name=='operation_node':
                            for child in value:
                                subop=pb.CMsgPaintKit_Operation();subop.header.defindex=ident
                                subop.operation_node.add().CopyFrom(child)
                                fields.extend(convert_operation(subop,trail))
                        elif field.name=='sticker':
                            for sticker in value:
                                fields.append(['sticker_base',resolve_var(sticker.base) if sticker.HasField('base') else ''])
                                fields.append(['sticker_weight',resolve_var(sticker.weight) if sticker.HasField('weight') else '1'])
                                fields.append(['sticker_spec',resolve_var(sticker.spec) if sticker.HasField('spec') else ''])
                        else:
                            for var in (value if field.label == field.LABEL_REPEATED else [value]):
                                if isinstance(var,pb.CMsgVarField):
                                    fields.append([field.name,resolve_var(var)])
                                else:raise AssetError('Unsupported paint field: '+field.name)
                    output.append([name,fields])
                else:raise AssetError('Invalid paint operation node')
            return output

        stages=convert_operation(root)
        if len(stages)!=1:raise AssetError('Paint operation has multiple roots')
        compositor.prepare_random(*stages[0])
        result=compositor.evaluate(*stages[0])
        compositor.warnings.append('Uses TF2 random streams and paint graph; material and sampling differences remain.')
        from .compositor import to_image
        return to_image(result)
