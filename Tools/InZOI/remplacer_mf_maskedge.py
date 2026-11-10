# -*- coding: utf-8 -*-
"""MF_MaskEdge (usure des bords des vêtements d'inZOI) n'est livré que compilé dans le ModKit : notre éditeur ne peut
pas l'utiliser et le matériau des vêtements (M_Char_Standard_Master, via MF_Weathering) ne compile pas.
On le remplace, dans Content/ImportedAssets/InZOIModKit (jamais sur Git), par une fonction équivalente : mêmes entrées
(Mask, Position, HalfWidth) et même sortie (Result), avec les identifiants attendus par MF_Weathering.
Result = bande autour de Position : saturate(1 - |Mask - Position| / HalfWidth).
Lancer avec UnrealEditor-Cmd ... -run=pythonscript -script=Tools/InZOI/remplacer_mf_maskedge.py (Unreal fermé)."""
import unreal

PATH = "/Game/ImportedAssets/InZOIModKit/BG/Material/MaterialFunction/MF_MaskEdge"
INPUTS = [("Mask", "C54B92E44356D42F6D96F2A09270E009", 0.0), ("Position", "EA4E6EB140760DC5E4A0D3A7EF09F50C", 0.5),
          ("HalfWidth", "3DA48C784F1BDA65807F639465254CFE", 0.1)]
OUTPUT = ("Result", "97E3429E4ECC7DFED1E5AEBB65B04F31")

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def guid(text):
    value, ok = unreal.GuidLibrary.parse_string_to_guid(text)
    return value


if EAL.does_asset_exist(PATH):
    EAL.delete_asset(PATH)
folder, name = PATH.rsplit("/", 1)
function = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
nodes = []
for index, (input_name, input_id, preview) in enumerate(INPUTS):
    node = MEL.create_material_expression_in_function(function, unreal.MaterialExpressionFunctionInput, -600, index * 150)
    node.set_editor_property("input_name", input_name)
    node.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
    unreal.InZOIImportLibrary.set_function_io_id(node, input_id)
    node.set_editor_property("sort_priority", index)
    # Valeur par défaut quand l'appel ne branche pas l'entrée (HalfWidth du 2e appel dans MF_Weathering)
    try:
        value = unreal.Vector4f()
        value.set_editor_property("x", preview)
        node.set_editor_property("preview_value", value)
        node.set_editor_property("use_preview_value_as_default", True)
    except Exception as error:
        unreal.log_warning("[ModKit] valeur par défaut de %s non posée (%s)" % (input_name, error))
    nodes.append(node)
custom = MEL.create_material_expression_in_function(function, unreal.MaterialExpressionCustom, -300, 100)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
custom.set_editor_property("code", "return saturate(1.0 - abs(M - P) / max(H, 0.0001));")
entries = []
for input_name in ("M", "P", "H"):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", input_name)
    entries.append(entry)
custom.set_editor_property("inputs", entries)
for node, pin in zip(nodes, ("M", "P", "H")):
    MEL.connect_material_expressions(node, "", custom, pin)
output = MEL.create_material_expression_in_function(function, unreal.MaterialExpressionFunctionOutput, 0, 100)
output.set_editor_property("output_name", OUTPUT[0])
unreal.InZOIImportLibrary.set_function_io_id(output, OUTPUT[1])
MEL.connect_material_expressions(custom, "", output, "")
MEL.update_material_function(function)
EAL.save_loaded_asset(function)
# Les matériaux qui l'utilisent, réenregistrés
for path in ("/Game/ImportedAssets/InZOIModKit/Characters/Materials/MaterialFunctions/Common/MF_Weather",
             "/Game/ImportedAssets/InZOIModKit/Characters/Materials/MaterialFunctions/Common/MF_Weathering",
             "/Game/ImportedAssets/InZOIModKit/Characters/Materials/MasterMaterials/Common/M_Char_Standard_Master"):
    asset = EAL.load_asset(path)
    if asset:
        if isinstance(asset, unreal.Material):
            MEL.recompile_material(asset)
        EAL.save_asset(path, False)
unreal.log("[ModKit] MF_MaskEdge remplacé")
