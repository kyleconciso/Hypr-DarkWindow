#include "WindowShader.h"

#include <hyprutils/string/String.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/config/ConfigValue.hpp>

static const std::map<std::string, std::tuple<std::string, Uniforms, IntroducesTransparency>> WINDOW_SHADER_SOURCES = {
    { "invert", { R"glsl(
        void windowShader(inout vec4 color) {
            color.rgb /= color.a;
            color.rgb = vec3(1.) - vec3(.88, .9, .92) * color.rgb;
            color.rgb = dot(vec3(0.26312, 0.5283, 0.10488), color.rgb) * 2.0 - color.rgb;
            color.rgb *= color.a;
        }
    )glsl", {}, {} } },

    { "tint", { R"glsl(
        uniform vec3 tintColor;
        uniform float tintStrength;

        void windowShader(inout vec4 color) {
            color.rgb /= color.a;
            color.rgb = color.rgb * (1.0 - tintStrength) + tintColor * tintStrength;
            color.rgb *= color.a;
        }
    )glsl", {
        { "tintColor", { 1, 0, 0 } },
        { "tintStrength", { 0.1 } },
    }, {} } },

    { "chromakey", { R"glsl(
        uniform vec3 bkg;
        uniform float similarity;
        uniform float amount;
        uniform float targetOpacity;

        void windowShader(inout vec4 color) {
            if (color.r >= bkg.r - similarity && color.r <= bkg.r + similarity &&
                    color.g >= bkg.g - similarity && color.g <= bkg.g + similarity &&
                    color.b >= bkg.b - similarity && color.b <= bkg.b + similarity) {
                vec3 error = vec3(abs(bkg.r - color.r), abs(bkg.g - color.g), abs(bkg.b - color.b));
                float avg_error = (error.r + error.g + error.b) / 3.0;

                color *= targetOpacity + (1.0 - targetOpacity) * avg_error * amount / similarity;
            }
        }
    )glsl", {
        { "bkg", { 0, 0, 0 } },
        { "similarity", { 0.1 } },
        { "amount", { 1.4 } },
        { "targetOpacity", { 0.83 } },
    }, IntroducesTransparency::Yes } },

    // --- YOUR CUSTOM SHADER ---
    { "chromablur", { R"glsl(
        uniform vec3 bkg;
        uniform float similarity;
        uniform float amount;
        uniform float targetOpacity;
        uniform float blurRadius;
        uniform float blurSteps;
        
        vec4 getChromaColor(vec4 inColor) {
            if (inColor.r >= bkg.r - similarity && inColor.r <= bkg.r + similarity &&
                inColor.g >= bkg.g - similarity && inColor.g <= bkg.g + similarity &&
                inColor.b >= bkg.b - similarity && inColor.b <= bkg.b + similarity) {
                
                vec3 error = vec3(abs(bkg.r - inColor.r), abs(bkg.g - inColor.g), abs(bkg.b - inColor.b));
                float avg_error = (error.r + error.g + error.b) / 3.0;

                inColor *= targetOpacity + (1.0 - targetOpacity) * avg_error * amount / similarity;
            }
            return inColor;
        }

        void windowShader(inout vec4 color) {
            if (blurRadius <= 0.00001) {
                color = getChromaColor(color);
                return;
            }

            vec4 sum = vec4(0.0);
            float totalWeight = 0.0;

            for (float x = -blurSteps; x <= blurSteps; x += 1.0) {
                for (float y = -blurSteps; y <= blurSteps; y += 1.0) {
                    vec2 offset = vec2(x, y) * blurRadius;
                    
                    vec4 pixColor = texture2D(tex, v_texcoord + offset);
                    
                    sum += getChromaColor(pixColor);
                    totalWeight += 1.0;
                }
            }

            color = sum / totalWeight;
        }
    )glsl", {
        { "bkg", { 0.172, 0.172, 0.172 } },
        { "similarity", { 0.15 } },
        { "amount", { 1.0 } },
        { "targetOpacity", { 0.4 } },
        { "blurRadius", { 0.002 } },
        { "blurSteps", { 2.0 } },
    }, IntroducesTransparency::Yes } },
};


std::optional<ShaderConfig*>& WindowShader::OnRenderWindowPre(PHLWINDOW window)
{
    m_ShadersSwapped.reset();

    auto ruleShader = m_RuleShadedWindows.find(window);
    auto dispatchShader = m_DispatchShadedWindows.find(window);
    if (ruleShader != m_RuleShadedWindows.end() && dispatchShader != m_DispatchShadedWindows.end())
    {
        if (ruleShader->second->ID != dispatchShader->second->ID)
            m_ShadersSwapped = dispatchShader->second;
    }
    else if (dispatchShader != m_DispatchShadedWindows.end())
        m_ShadersSwapped = dispatchShader->second;
    else if (ruleShader != m_RuleShadedWindows.end())
        m_ShadersSwapped = ruleShader->second;

    if (m_ShadersSwapped) {
        (*m_ShadersSwapped)->CompiledShaders->ApplyArgs((*m_ShadersSwapped)->Args);

        std::swap((*m_ShadersSwapped)->CompiledShaders->EXT, g_pHyprOpenGL->m_shaders->m_shEXT);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBA, g_pHyprOpenGL->m_shaders->m_shRGBA);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBX, g_pHyprOpenGL->m_shaders->m_shRGBX);
        std::swap((*m_ShadersSwapped)->CompiledShaders->CM, g_pHyprOpenGL->m_shaders->m_shCM);
    }

    return m_ShadersSwapped;
}

void WindowShader::OnRenderWindowPost()
{
    if (m_ShadersSwapped)
    {
        std::swap((*m_ShadersSwapped)->CompiledShaders->EXT, g_pHyprOpenGL->m_shaders->m_shEXT);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBA, g_pHyprOpenGL->m_shaders->m_shRGBA);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBX, g_pHyprOpenGL->m_shaders->m_shRGBX);
        std::swap((*m_ShadersSwapped)->CompiledShaders->CM, g_pHyprOpenGL->m_shaders->m_shCM);
        m_ShadersSwapped.reset();
    }
}

void WindowShader::Unload()
{
    OnRenderWindowPost();

    m_Shaders.clear();
}

void WindowShader::ShadeIfMatches(PHLWINDOW window)
{
    if (!window) return;

    std::optional<std::string> shader;
    auto& props = window->m_ruleApplicator->m_otherProps.props;

    if (const auto& it = props.find(m_RuleShade); it != props.end())
        shader = it->second->effect;

    auto windowIt = m_RuleShadedWindows.find(window);
    std::optional<std::string> currentShader;
    if (windowIt != m_RuleShadedWindows.end()) currentShader = windowIt->second->ID;

    if (shader != currentShader)
    {
        if (shader)
            m_RuleShadedWindows[window] = EnsureShader(*shader);
        else
            m_RuleShadedWindows.erase(window);

        g_pHyprRenderer->damageWindow(window);
    }
}


void WindowShader::ToggleShade(PHLWINDOW window, const std::string& shader)
{
    if (!window)
        return;

    auto windowIt = m_DispatchShadedWindows.find(window);
    std::optional<std::string> currentShader;
    if (windowIt != m_DispatchShadedWindows.end()) currentShader = windowIt->second->ID;

    if (std::optional(shader) != currentShader)
        m_DispatchShadedWindows[window] = EnsureShader(shader);
    else
        m_DispatchShadedWindows.erase(window);

    g_pHyprRenderer->damageWindow(window);
}

void WindowShader::ForgetWindow(PHLWINDOW window)
{
    m_RuleShadedWindows.erase(window);
    m_DispatchShadedWindows.erase(window);
}

void WindowShader::ReshadeWindows()
{
    m_RuleShadedWindows = {};

    for (const auto& window : g_pCompositor->m_windows)
        ShadeIfMatches(window);
}


void WindowShader::AddPredefinedShader(const std::string& name)
{
    static const auto add = [](WindowShader* self, const auto& source) {
        if (self->m_Shaders.contains(source.first)) return;
        auto& [id, options] = source;

        Debug::log(INFO, "Loading predefined shader with name: {}", id);

        UP<ShaderConfig> shader(new ShaderConfig{
            .ID = id,
            .CompiledShaders = SP(new ShaderHolder(std::get<std::string>(options))),
            .Args = std::get<Uniforms>(options),
            .Transparent = std::get<IntroducesTransparency>(options),
        });
        shader->CompiledShaders->PrimeUniforms(shader->Args);

        self->m_Shaders[source.first] = std::move(shader);
    };

    if (name == "all")
    {
        for (const auto& source : WINDOW_SHADER_SOURCES)
        {
            add(this, source);
        }
    }
    else
    {
        auto source = WINDOW_SHADER_SOURCES.find(name);
        if (source == WINDOW_SHADER_SOURCES.end())
            throw efmt("Predefined shader with name {} not found", name);

        add(this, *source);
    }
}

ShaderConfig* WindowShader::AddShader(ShaderDefinition def)
{
    auto found = m_Shaders.find(def.ID);
    if (found != m_Shaders.end()) return found->second.get();

    Debug::log(INFO, "Loading custom shader with id: {}", def.ID);

    UP<ShaderConfig> shader(new ShaderConfig{ .ID = def.ID });

    if (def.From != "")
    {
        if (!m_Shaders.contains(def.From))
            throw efmt("Shader with ID {} has unknown .from shader", def.ID);

        auto& from = m_Shaders[def.From];
        shader->Args = from->Args;
        shader->CompiledShaders = from->CompiledShaders;
        shader->Transparent = from->Transparent;
    }

    if (def.Path != "")
    {
        std::ifstream file(def.Path);
        std::string source = std::string(std::istreambuf_iterator(file), {});

        shader->CompiledShaders = SP(new ShaderHolder(source));
    }

    if (!shader->CompiledShaders)
        throw efmt("Either .from or .path has to be set for Shader with ID {}", def.ID);


    shader->CompiledShaders->PrimeUniforms(def.Args);
    for (auto& [arg, val] : def.Args)
    {
        shader->Args[arg] = val;
    }

    if (def.Transparency) shader->Transparent = IntroducesTransparency::Yes;

    auto ret = shader.get();
    m_Shaders[def.ID] = std::move(shader);
    return ret;
}

ShaderConfig* WindowShader::EnsureShader(const std::string& shader)
{
    size_t space = shader.find(" ");
    if (space == std::string::npos)
    {
        auto found = m_Shaders.find(shader);
        if (found == m_Shaders.end())
            throw efmt("Unable to find shader {}", shader);

        return found->second.get();
    }
    else
    {
        auto from = Hyprutils::String::trim(shader.substr(0, space));
        auto args = shader.substr(space + 1);
        return AddShader({ shader, from, {}, args, {} });
    }
}
