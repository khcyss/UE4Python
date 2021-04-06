import unreal_engine as ue
from unreal_engine.classes import BlueprintPathsLibrary
import os
from unreal_engine import FbxImporter, FbxMesh, FbxNode, FbxScene, FbxObject
from unreal_engine.classes import FbxImportUI, StaticMesh, Class, Object
from unreal_engine.classes import PyFactory


class MyFbxFactory(PyFactory):
    def __init__(self):
        self.bEditorImport = True
        self.PyDisplayName = 'MyFBXFactory'
        self.Formats = ['fbx;FBX']
        self.SupportedClass = StaticMesh
        self.ImportPriority = 120

    def PyFactoryCreateFile(self, class_: Class, parent: Object, name: str, filename: str) -> Object:
        newimporter = FbxImporter()
        newimporter.initialize('C:/Users/VR/Desktop/112412.FBX')
        return None

    def ScriptFactoryCanImport(self, filename: str) -> bool:
        if 'fbx' or 'FBX' in str(BlueprintPathsLibrary.GetExtension(filename)):
            return True
        else:
            return False

