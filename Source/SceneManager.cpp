///////////////////////////////////////////////////////////////////////////////
// SceneManager.cpp
//
// Scene setup and rendering for my final milestone project.
//
// Responsibilities
// - Load textures with stb_image and bind them to texture units
// - Push model transforms, material values, and lighting uniforms to the shader
// - Draw the floor, backdrop wall, and the objects in my scene
//
// Shader behavior note
// The provided fragment shader always loops across lightSources[0..3].
// It also applies the material ambient contribution inside that loop.
// Because of that, the effective ambient term becomes:
//   ambientStrength * TOTAL_LIGHTS
// To keep the scene from washing out, I divide the ambientStrength I want
// by TOTAL_LIGHTS before uploading it to the shader.
//
// Lighting plan
// - Light 0: neutral white back light for separation/rim highlights
// - Light 1: red front light to make the color tint obvious
// - Light 2-3: disabled (kept zeroed, but still uploaded because the shader loops)
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>
#include <iostream>
#include <string>

namespace
{
    // These names must match the provided shader uniforms.
    constexpr const char* kUniformModel = "model";
    constexpr const char* kUniformObjectColor = "objectColor";
    constexpr const char* kUniformTexture = "objectTexture";
    constexpr const char* kUniformUseTexture = "bUseTexture";
    constexpr const char* kUniformUseLighting = "bUseLighting";

    // The provided fragment shader uses TOTAL_LIGHTS = 4 and always loops all of them.
    constexpr int kTotalLights = 4;

    // CPU-side mirror of the shader LightSource struct fields I actually upload.
    struct LightSourceCPU
    {
        glm::vec3 position;
        glm::vec3 ambientColor;
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;
        float focalStrength;
        float specularIntensity;
    };

    // Builds uniform names like "lightSources[1].diffuseColor".
    std::string LightUniform(const int index, const char* field)
    {
        return "lightSources[" + std::to_string(index) + "]." + field;
    }

    // The shader adds ambient once per light, so I divide by TOTAL_LIGHTS here.
    float AmbientPerLight(const float intendedAmbientStrength)
    {
        return intendedAmbientStrength / static_cast<float>(kTotalLights);
    }
}

/***********************************************************
 *  SceneManager()
 *
 *  Initializes the scene manager with the shader manager that
 *  owns the compiled shader program.
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
    : m_pShaderManager(pShaderManager),
    m_basicMeshes(new ShapeMeshes()),
    m_loadedTextures(0)
{
}

/***********************************************************
 *  ~SceneManager()
 *
 *  Releases textures and mesh helpers owned by this class.
 ***********************************************************/
SceneManager::~SceneManager()
{
    DestroyGLTextures();

    delete m_basicMeshes;
    m_basicMeshes = NULL;

    m_pShaderManager = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  Loads an image from disk and uploads it to OpenGL as a 2D texture.
 *  - Generates mipmaps to reduce shimmering when moving the camera
 *  - Uses repeat wrap so tiled UVs behave as expected
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
    if (m_loadedTextures >= 16)
    {
        std::cout << "Texture limit reached (16). Could not load: " << filename << std::endl;
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    GLuint textureID = 0;

    // Milestone UVs expect bottom-left image origin.
    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (!image)
    {
        std::cout << "Could not load image: " << filename << std::endl;
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (channels == 3)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    }
    else if (channels == 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    }
    else
    {
        std::cout << "Unsupported channel count (" << channels << ") for: " << filename << std::endl;
        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &textureID);
        return false;
    }

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textureIDs[m_loadedTextures].ID = textureID;
    m_textureIDs[m_loadedTextures].tag = tag;
    ++m_loadedTextures;

    return true;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  Binds each loaded texture to a consecutive texture unit.
 *  The shader selects the right unit by setting the sampler uniform.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
    }
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  Deletes all OpenGL textures created by this scene manager.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        GLuint id = static_cast<GLuint>(m_textureIDs[i].ID);
        if (id != 0)
        {
            glDeleteTextures(1, &id);
            m_textureIDs[i].ID = 0;
        }
    }
    m_loadedTextures = 0;
}

/***********************************************************
 *  FindTextureID()
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        if (m_textureIDs[i].tag.compare(tag) == 0)
        {
            return m_textureIDs[i].ID;
        }
    }
    return -1;
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  Returns the texture unit index for a tag (0..m_loadedTextures-1).
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; ++i)
    {
        if (m_textureIDs[i].tag.compare(tag) == 0)
        {
            return i;
        }
    }
    return -1;
}

/***********************************************************
 *  FindMaterial()
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
    for (size_t i = 0; i < m_objectMaterials.size(); ++i)
    {
        if (m_objectMaterials[i].tag == tag)
        {
            material = m_objectMaterials[i];
            return true;
        }
    }
    return false;
}

/***********************************************************
 *  SetTransformations()
 *
 *  Builds the model matrix (scale, rotate, translate) and uploads it.
 ***********************************************************/
void SceneManager::SetTransformations(
    glm::vec3 scaleXYZ,
    float XrotationDegrees,
    float YrotationDegrees,
    float ZrotationDegrees,
    glm::vec3 positionXYZ)
{
    glm::mat4 scale = glm::scale(scaleXYZ);
    glm::mat4 rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 translation = glm::translate(positionXYZ);

    glm::mat4 model = translation * rotationX * rotationY * rotationZ * scale;

    if (m_pShaderManager)
    {
        m_pShaderManager->setMat4Value(kUniformModel, model);
    }
}

/***********************************************************
 *  SetShaderColor()
 *
 *  Disables texturing and draws using a solid RGBA color.
 ***********************************************************/
void SceneManager::SetShaderColor(float r, float g, float b, float a)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(kUniformUseTexture, false);
        m_pShaderManager->setVec4Value(kUniformObjectColor, glm::vec4(r, g, b, a));
    }
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  Enables texturing and selects the texture slot by tag.
 *  If the tag is missing, I fall back to a neutral gray.
 ***********************************************************/
void SceneManager::SetShaderTexture(std::string textureTag)
{
    if (!m_pShaderManager)
        return;

    const int slot = FindTextureSlot(textureTag);
    if (slot < 0)
    {
        std::cout << "Texture tag not found: " << textureTag << std::endl;
        m_pShaderManager->setIntValue(kUniformUseTexture, false);
        m_pShaderManager->setVec4Value(kUniformObjectColor, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
        return;
    }

    m_pShaderManager->setIntValue(kUniformUseTexture, true);
    m_pShaderManager->setVec4Value(kUniformObjectColor, glm::vec4(1.0f));
    m_pShaderManager->setSampler2DValue(kUniformTexture, slot);
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  Controls texture tiling by scaling the UVs in the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
    }
}

/***********************************************************
 *  SetShaderMaterial()
 ***********************************************************/
void SceneManager::SetShaderMaterial(std::string materialTag)
{
    if (!m_pShaderManager)
        return;

    OBJECT_MATERIAL mat{};
    if (!FindMaterial(materialTag, mat))
    {
        std::cout << "Material tag not found: " << materialTag << std::endl;
        return;
    }

    m_pShaderManager->setFloatValue("material.ambientStrength", mat.ambientStrength);
    m_pShaderManager->setVec3Value("material.ambientColor", mat.ambientColor);
    m_pShaderManager->setVec3Value("material.diffuseColor", mat.diffuseColor);
    m_pShaderManager->setVec3Value("material.specularColor", mat.specularColor);
    m_pShaderManager->setFloatValue("material.shininess", mat.shininess);
}

///////////////////////////////////////////////////////////////////////////////
// Lighting
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  SetSceneLights()
 *
 *  The shader always loops across kTotalLights, so I upload all 4.
 *  Lights 2 and 3 are set to zero so they do not contribute.
 ***********************************************************/
void SceneManager::SetSceneLights()
{
    if (!m_pShaderManager)
        return;

    const LightSourceCPU lights[kTotalLights] =
    {
        // [0] White back light (rim/separation)
        {
            glm::vec3(0.0f, 7.0f, -12.0f),
            glm::vec3(0.008f, 0.008f, 0.008f),
            glm::vec3(0.120f, 0.120f, 0.120f),
            glm::vec3(0.070f, 0.070f, 0.070f),
            28.0f,
            0.60f
        },

        // [1] Red front light (clear color tint on visible faces)
        {
            glm::vec3(0.0f, 4.0f, 9.0f),
            glm::vec3(0.004f, 0.000f, 0.000f),
            glm::vec3(0.420f, 0.010f, 0.010f),
            glm::vec3(0.120f, 0.010f, 0.010f),
            20.0f,
            0.85f
        },

        // [2] Disabled
        {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f),
            1.0f,
            0.0f
        },

        // [3] Disabled
        {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f),
            1.0f,
            0.0f
        }
    };

    for (int i = 0; i < kTotalLights; ++i)
    {
        m_pShaderManager->setVec3Value(LightUniform(i, "position").c_str(), lights[i].position);
        m_pShaderManager->setVec3Value(LightUniform(i, "ambientColor").c_str(), lights[i].ambientColor);
        m_pShaderManager->setVec3Value(LightUniform(i, "diffuseColor").c_str(), lights[i].diffuseColor);
        m_pShaderManager->setVec3Value(LightUniform(i, "specularColor").c_str(), lights[i].specularColor);
        m_pShaderManager->setFloatValue(LightUniform(i, "focalStrength").c_str(), lights[i].focalStrength);
        m_pShaderManager->setFloatValue(LightUniform(i, "specularIntensity").c_str(), lights[i].specularIntensity);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Material presets
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  Material presets
 *
 *  Ambient is uploaded using AmbientPerLight() to compensate for how the
 *  provided shader applies the ambient term once per light.
 ***********************************************************/
void SceneManager::ApplystainedglassMaterial()
{
    if (!m_pShaderManager) return;

    m_pShaderManager->setVec3Value("material.ambientColor", glm::vec3(1.0f));
    m_pShaderManager->setFloatValue("material.ambientStrength", AmbientPerLight(0.12f));
    m_pShaderManager->setVec3Value("material.diffuseColor", glm::vec3(0.80f));
    m_pShaderManager->setVec3Value("material.specularColor", glm::vec3(0.10f));
    m_pShaderManager->setFloatValue("material.shininess", 18.0f);
}

void SceneManager::ApplyRubberMaterial()
{
    if (!m_pShaderManager) return;

    m_pShaderManager->setVec3Value("material.ambientColor", glm::vec3(1.0f));
    m_pShaderManager->setFloatValue("material.ambientStrength", AmbientPerLight(0.28f));
    m_pShaderManager->setVec3Value("material.diffuseColor", glm::vec3(1.0f));
    m_pShaderManager->setVec3Value("material.specularColor", glm::vec3(0.05f));
    m_pShaderManager->setFloatValue("material.shininess", 10.0f);
}

void SceneManager::ApplyWoodMaterial()
{
    if (!m_pShaderManager) return;

    m_pShaderManager->setVec3Value("material.ambientColor", glm::vec3(1.0f));
    m_pShaderManager->setFloatValue("material.ambientStrength", AmbientPerLight(0.22f));
    m_pShaderManager->setVec3Value("material.diffuseColor", glm::vec3(1.0f));
    m_pShaderManager->setVec3Value("material.specularColor", glm::vec3(0.10f));
    m_pShaderManager->setFloatValue("material.shininess", 18.0f);
}

void SceneManager::ApplyMetalMaterial()
{
    if (!m_pShaderManager) return;

    m_pShaderManager->setVec3Value("material.ambientColor", glm::vec3(1.0f));
    m_pShaderManager->setFloatValue("material.ambientStrength", AmbientPerLight(0.10f));
    m_pShaderManager->setVec3Value("material.diffuseColor", glm::vec3(0.95f));
    m_pShaderManager->setVec3Value("material.specularColor", glm::vec3(0.28f));
    m_pShaderManager->setFloatValue("material.shininess", 38.0f);
}

void SceneManager::ApplyBrickMaterial()
{
    if (!m_pShaderManager) return;

    m_pShaderManager->setVec3Value("material.ambientColor", glm::vec3(1.0f));
    m_pShaderManager->setFloatValue("material.ambientStrength", AmbientPerLight(0.20f));
    m_pShaderManager->setVec3Value("material.diffuseColor", glm::vec3(0.95f));
    m_pShaderManager->setVec3Value("material.specularColor", glm::vec3(0.08f));
    m_pShaderManager->setFloatValue("material.shininess", 12.0f);
}

///////////////////////////////////////////////////////////////////////////////
// Scene setup
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  PrepareScene()
 *
 *  Loads the primitive meshes and the texture set used by the scene.
 ***********************************************************/
void SceneManager::PrepareScene()
{
    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadCylinderMesh();
    m_basicMeshes->LoadTorusMesh();
    m_basicMeshes->LoadBoxMesh();
    m_basicMeshes->LoadSphereMesh();

    CreateGLTexture("Textures/wood.jpg", "wood");
    CreateGLTexture("Textures/stainedglass.jpg", "stainedglass");
    CreateGLTexture("Textures/rubber.jpg", "rubber");
    CreateGLTexture("Textures/stainless.jpg", "stainless");
    CreateGLTexture("Textures/stainless_end.jpg", "stainless_end");
    CreateGLTexture("Textures/rusticwood.jpg", "rusticwood");
    CreateGLTexture("Textures/gold-seamless-texture.jpg", "gold");
    CreateGLTexture("Textures/backdrop.jpg", "backdrop");

    BindGLTextures();
}

///////////////////////////////////////////////////////////////////////////////
// Object builders
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  BuildMug()
 ***********************************************************/
void SceneManager::BuildMug(glm::vec3 positionXYZ)
{
    const float bodyHeight = 1.30f;
    const float bodyRadius = 0.50f;

    const float baseHeight = 0.06f;
    const float baseRadius = 0.54f;

    const glm::vec3 handleScale(0.34f, 0.34f, 0.14f);
    const glm::vec3 handleOffset(bodyRadius + 0.30f, 0.50f, 0.0f);

    const float baseHalf = baseHeight * 0.5f;
    const float bodyHalf = bodyHeight * 0.5f;

    const float overlap = 0.03f;

    // The cylinder mesh in the provided template sits a little high visually.
    // I drop the body and handle together so the mug reads as grounded.
    const float bodyDrop = 0.6f;

    const float baseCenterY = baseHalf;

    SetShaderTexture("rubber");
    SetTextureUVScale(2.0f, 2.0f);
    ApplyRubberMaterial();

    SetTransformations(
        glm::vec3(baseRadius, baseHeight, baseRadius),
        0.0f, 0.0f, 0.0f,
        positionXYZ + glm::vec3(0.0f, baseCenterY, 0.0f));
    m_basicMeshes->DrawCylinderMesh();

    const float bodyCenterY = baseCenterY + baseHalf + bodyHalf - overlap;
    const glm::vec3 bodyPos = positionXYZ + glm::vec3(0.0f, bodyCenterY - bodyDrop, 0.0f);

    SetShaderTexture("stainedglass");
    SetTextureUVScale(0.8f, 0.8f);
    ApplystainedglassMaterial();

    SetTransformations(
        glm::vec3(bodyRadius, bodyHeight, bodyRadius),
        0.0f, 0.0f, 0.0f,
        bodyPos);
    m_basicMeshes->DrawCylinderMesh();

    SetShaderTexture("rubber");
    SetTextureUVScale(1.4f, 1.4f);
    ApplyRubberMaterial();

    SetTransformations(
        handleScale,
        0.0f, 0.0f, 90.0f,
        bodyPos + handleOffset);
    m_basicMeshes->DrawTorusMesh();
}

/***********************************************************
 *  BuildCan()
 ***********************************************************/
void SceneManager::BuildCan(glm::vec3 positionXYZ)
{
    const float bodyRadius = 0.45f;
    const float bodyHeight = 1.20f;

    const float topRadius = 0.46f;
    const float topHeight = 0.05f;

    const float overlap = 0.01f;

    const float bodyHalf = bodyHeight * 0.5f;
    const float topHalf = topHeight * 0.5f;

    const float bodyCenterY = bodyHalf;
    const float topCenterY = bodyHeight + topHalf - overlap;

    SetShaderTexture("gold");
    SetTextureUVScale(1.0f, 1.0f);
    ApplyMetalMaterial();

    SetTransformations(
        glm::vec3(bodyRadius, bodyHeight, bodyRadius),
        0.0f, 0.0f, 0.0f,
        positionXYZ + glm::vec3(0.0f, bodyCenterY, 0.0f));
    m_basicMeshes->DrawCylinderMesh();

    SetShaderTexture("stainless_end");
    SetTextureUVScale(1.0f, 1.0f);
    ApplyMetalMaterial();

    SetTransformations(
        glm::vec3(topRadius, topHeight, topRadius),
        0.0f, 0.0f, 0.0f,
        positionXYZ + glm::vec3(0.0f, topCenterY, 0.0f));
    m_basicMeshes->DrawCylinderMesh();
}

/***********************************************************
 *  BuildCoaster()
 ***********************************************************/
void SceneManager::BuildCoaster(glm::vec3 positionXYZ)
{
    SetShaderTexture("gold");
    SetTextureUVScale(1.0f, 1.0f);
    ApplyMetalMaterial();

    SetTransformations(
        glm::vec3(0.90f, 0.05f, 0.90f),
        0.0f, 0.0f, 0.0f,
        positionXYZ + glm::vec3(0.0f, 0.025f, 0.0f));
    m_basicMeshes->DrawCylinderMesh();
}

/***********************************************************
 *  BuildWoodBlock()
 ***********************************************************/
void SceneManager::BuildWoodBlock(glm::vec3 positionXYZ)
{
    SetShaderTexture("rusticwood");
    SetTextureUVScale(1.0f, 1.0f);
    ApplyBrickMaterial();

    SetTransformations(
        glm::vec3(1.2f, 0.35f, 0.7f),
        0.0f, 25.0f, 0.0f,
        positionXYZ + glm::vec3(0.0f, 0.175f, 0.0f));
    m_basicMeshes->DrawBoxMesh();
}

/***********************************************************
 *  BuildBackdrop()
 ***********************************************************/
void SceneManager::BuildBackdrop(glm::vec3 positionXYZ)
{
    SetShaderTexture("backdrop");
    SetTextureUVScale(2.0f, 2.0f);
    ApplyWoodMaterial();

    SetTransformations(
        glm::vec3(60.0f, 1.0f, 16.0f),
        90.0f, 0.0f, 0.0f,
        positionXYZ);
    m_basicMeshes->DrawPlaneMesh();
}

///////////////////////////////////////////////////////////////////////////////
// Rendering
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  RenderScene()
 ***********************************************************/
void SceneManager::RenderScene()
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(kUniformUseLighting, true);
        SetSceneLights();
    }

    SetTransformations(
        glm::vec3(60.0f, 1.0f, 60.0f),
        0.0f, 0.0f, 0.0f,
        glm::vec3(0.0f, 0.0f, -15.0f));

    SetShaderTexture("wood");
    SetTextureUVScale(10.0f, 10.0f);
    ApplyWoodMaterial();
    m_basicMeshes->DrawPlaneMesh();

    BuildBackdrop(glm::vec3(0.0f, 10.0f, -25.0f));

    BuildCoaster(glm::vec3(-2.0f, 0.0f, -1.0f));
    BuildMug(glm::vec3(-2.0f, 0.0f, -1.0f));

    BuildCan(glm::vec3(2.0f, -0.55f, -1.0f));

    BuildWoodBlock(glm::vec3(0.0f, 0.0f, 1.7f));

    SetShaderTexture("stainless");
    SetTextureUVScale(1.0f, 1.0f);
    ApplyMetalMaterial();

    SetTransformations(
        glm::vec3(0.35f, 0.35f, 0.35f),
        0.0f, 0.0f, 0.0f,
        glm::vec3(-0.8f, 0.35f, 0.6f));
    m_basicMeshes->DrawSphereMesh();
}