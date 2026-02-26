///////////////////////////////////////////////////////////////////////////////
// SceneManager.h
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "ShapeMeshes.h"

#include <string>
#include <vector>

/***********************************************************
 *  SceneManager
 *
 *  Prepares and renders the 3D scene by:
 *  - loading textures
 *  - configuring lighting/material shader uniforms
 *  - drawing meshes with transforms
 ***********************************************************/
class SceneManager
{
public:
    SceneManager(ShaderManager* pShaderManager);
    ~SceneManager();

    struct TEXTURE_INFO
    {
        std::string tag;
        uint32_t ID;
    };

    struct OBJECT_MATERIAL
    {
        float ambientStrength;
        glm::vec3 ambientColor;
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;
        float shininess;
        std::string tag;
    };

public:
    void PrepareScene();
    void RenderScene();

private:
    ShaderManager* m_pShaderManager;
    ShapeMeshes* m_basicMeshes;

    int m_loadedTextures;
    TEXTURE_INFO m_textureIDs[16];
    std::vector<OBJECT_MATERIAL> m_objectMaterials;

    // Texture utilities
    bool CreateGLTexture(const char* filename, std::string tag);
    void BindGLTextures();
    void DestroyGLTextures();
    int FindTextureID(std::string tag);
    int FindTextureSlot(std::string tag);

    // Material utilities (optional tag-based materials)
    bool FindMaterial(std::string tag, OBJECT_MATERIAL& material);
    void SetShaderMaterial(std::string materialTag);

    // Transform + render state utilities
    void SetTransformations(
        glm::vec3 scaleXYZ,
        float XrotationDegrees,
        float YrotationDegrees,
        float ZrotationDegrees,
        glm::vec3 positionXYZ);

    void SetShaderColor(float red, float green, float blue, float alpha);
    void SetShaderTexture(std::string textureTag);
    void SetTextureUVScale(float u, float v);

    // Lighting
    void SetSceneLights();

    // Material presets (simple, reusable uniform setters)
    void ApplystainedglassMaterial();
    void ApplyRubberMaterial();
    void ApplyWoodMaterial();
    void ApplyMetalMaterial();
    void ApplyBrickMaterial();

    // Object builders
    void BuildMug(glm::vec3 positionXYZ);
    void BuildCan(glm::vec3 positionXYZ);
    void BuildCoaster(glm::vec3 positionXYZ);
    void BuildWoodBlock(glm::vec3 positionXYZ);
    void BuildBackdrop(glm::vec3 positionXYZ);
};