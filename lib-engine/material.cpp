#include "material.hpp"
#include "fwd_renderer.hpp"

using namespace polymer;

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

void MetallicRoughnessMaterial::update_uniforms()
{
    bindpoint = 0;

    auto & shader = compiled_variant->shader;
    shader.bind();

    shader.uniform("u_roughness", roughnessFactor);
    shader.uniform("u_metallic", metallicFactor);
    shader.uniform("u_opacity", opacity);
    shader.uniform("u_albedo", baseAlbedo);
    shader.uniform("u_emissive", baseEmissive);
    shader.uniform("u_specularLevel", specularLevel);
    shader.uniform("u_occlusionStrength", occlusionStrength);
    shader.uniform("u_ambientStrength", ambientStrength);
    shader.uniform("u_emissiveStrength", emissiveStrength);
    shader.uniform("u_shadowOpacity", shadowOpacity);
    shader.uniform("u_texCoordScale", float2(texcoordScale));

    if (compiled_variant->enabled("HAS_ALBEDO_MAP")) shader.texture("s_albedo", bindpoint++, albedo.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_NORMAL_MAP")) shader.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_ROUGHNESS_MAP")) shader.texture("s_roughness", bindpoint++, roughness.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_METALNESS_MAP")) shader.texture("s_metallic", bindpoint++, metallic.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_EMISSIVE_MAP")) shader.texture("s_emissive", bindpoint++, emissive.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_HEIGHT_MAP")) shader.texture("s_height", bindpoint++, height.get(), GL_TEXTURE_2D);
    if (compiled_variant->enabled("HAS_OCCLUSION_MAP")) shader.texture("s_occlusion", bindpoint++, occlusion.get(), GL_TEXTURE_2D);

    shader.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_ibl(GLuint irradiance, GLuint radiance)
{
    auto & shader = compiled_variant->shader;
    if (!compiled_variant->enabled("USE_IMAGE_BASED_LIGHTING")) throw std::runtime_error("should not be called unless USE_IMAGE_BASED_LIGHTING is defined.");

    shader.bind();
    shader.texture("sc_irradiance", bindpoint++, irradiance, GL_TEXTURE_CUBE_MAP);
    shader.texture("sc_radiance", bindpoint++, radiance, GL_TEXTURE_CUBE_MAP);
    shader.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_shadow(GLuint handle)
{
    auto & shader = compiled_variant->shader;
    if (!compiled_variant->enabled("ENABLE_SHADOWS")) throw std::runtime_error("should not be called unless ENABLE_SHADOWS is defined.");

    shader.bind();
    shader.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
    shader.unbind();
}

void MetallicRoughnessMaterial::use()
{
    if (!compiled_variant)
    {
        compiled_variant = shader.get()->get_variant({ "TWO_CASCADES", "USE_PCF_3X3", "ENABLE_SHADOWS", "USE_IMAGE_BASED_LIGHTING",
            "HAS_ROUGHNESS_MAP", "HAS_METALNESS_MAP", "HAS_ALBEDO_MAP", "HAS_NORMAL_MAP", "HAS_OCCLUSION_MAP" });
    }
    compiled_variant->shader.bind();
}
