import unreal_engine as ue
from unreal_engine.classes import BlueprintPathsLibrary
import os
from unreal_engine import FbxImporter, FbxMesh, FbxNode, FbxScene, FbxObject, FbxManager, FbxIOSettings
from unreal_engine.classes import FbxImportUI, StaticMesh, Class, Object, PyFactory


class MyFbxFactory(PyFactory):
    def __init__(self):
        self.bEditorImport = True
        self.PyDisplayName = 'MyFBXFactory'
        self.Formats = ['fbx;FBX']
        self.SupportedClass = StaticMesh
        self.ImportPriority = 120
        self.manager = FbxManager()
        self.io_settings = FbxIOSettings(self.manager, 'IOSROOT')
        self.manager.set_io_settings(self.io_settings)
        self.importer = FbxImporter(self.manager, 'importer')
        self.scene = FbxScene(self.manager, 'scene')

    def get_objects_by_class(self, name):
        objects = []
        for i in range(0, self.scene.get_src_object_count()):
            obj = self.scene.get_src_object(i)
            if obj.get_class_name() == name:
                objects.append(obj)
        return objects

    def PyFactoryCreateFile(self, uclass: Class, parent: Object, name: str, filename: str) -> Object:
        self.importer.initialize(filename, self.io_settings)
        self.importer._import(self.scene)
        tempffbx = self.get_objects_by_class('FbxImporter')
        tempinstance = tempffbx._get_instance()
        tempinstance._import_from_file(filename, 'fbx', True)

        return None

    def ScriptFactoryCanImport(self, filename: str) -> bool:
        if 'fbx' or 'FBX' in str(BlueprintPathsLibrary.GetExtension(filename)):
            return True
        else:
            return False

