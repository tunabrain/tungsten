TEMPLATE = lib
CONFIG += staticlib
TARGET = core

include(../../shared.pri)

SOURCES += \
    ../bsdfs/MixedBsdf.cpp \
    ../Debug.cpp \
    ../extern/lodepng/lodepng.cpp \
    ../extern/stbi/stb_image.c \
    ../extern/sobol/sobol.cpp \
    ../io/FileUtils.cpp \
    ../io/JsonUtils.cpp \
    ../io/ObjLoader.cpp \
    ../io/Scene.cpp \
    ../materials/AtmosphericScattering.cpp \
    ../materials/Material.cpp \
    ../math/Mat4f.cpp \
    ../primitives/Mesh.cpp
    
HEADERS += \
    ../bsdfs/Bsdf.hpp \
    ../bsdfs/DielectricBsdf.hpp \
    ../bsdfs/LambertBsdf.hpp \
    ../bsdfs/MixedBsdf.hpp \
    ../bsdfs/PhongBsdf.hpp \
    ../extern/lodepng/lodepng.h \
    ../extern/stbi/stb_image.h \
    ../extern/sobol/sobol.h \
    ../Config.hpp \
    ../Camera.hpp \
    ../Entity.hpp \
    ../Debug.hpp \
    ../IntTypes.hpp \
    ../io/FileUtils.hpp \
    ../io/HfaLoader.hpp \
    ../io/JsonSerializable.hpp \
    ../io/JsonUtils.hpp \
    ../io/MeshInputOutput.hpp \
    ../io/ObjLoader.hpp \
    ../io/ObjMaterial.hpp \
    ../io/Scene.hpp \
    ../lights/ConeLight.hpp \
    ../lights/DirectionalLight.hpp \
    ../lights/EnvironmentLight.hpp \
    ../lights/Light.hpp \
    ../lights/PointLight.hpp \
    ../lights/QuadLight.hpp \
    ../lights/SphereLight.hpp \
    ../materials/AtmosphericScattering.hpp \
    ../materials/Material.hpp \
    ../materials/Texture.hpp \
    ../math/Angle.hpp \
    ../math/BitManip.hpp \
    ../math/Mat4f.hpp \
    ../math/MathUtil.hpp \
    ../math/TangentSpace.hpp \
    ../math/Vec.hpp \
    ../primitives/HeightmapTerrain.hpp \
    ../primitives/Mesh.hpp \
    ../primitives/PackedGeometry.hpp \
    ../primitives/Triangle.hpp \
    ../primitives/Vertex.hpp \
    ../sampling/LightSample.hpp \
    ../sampling/Sample.hpp \
    ../sampling/SampleGenerator.hpp \
    ../sampling/ScatterEvent.hpp

DEPENDPATH += HEADERS