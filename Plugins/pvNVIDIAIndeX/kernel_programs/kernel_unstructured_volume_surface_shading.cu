/******************************************************************************
 * Copyright 2025 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/

// # RTC Kernel:
// ** Unstructured Volume Surface Shading **

// # Summary:
// Render the unstructured volume and optionally apply local surface shading at an user selected scalar threshold

NV_IDX_XAC_VERSION_1_0

// DON'T CHANGE THIS STRUCT !!!
// It maps the GUI parameters from the Custom Visual Element GUI to this kernel.
// floats maps {pfloat1, pfloat2, pfloat3, pfloat4} GUI Parameters.
// ints maps {pint1, pint2, pint3, pint4} GUI Parameters.
struct Custom_params
{
    float4 floats;  // floats array
    int4 ints;      // ints array
};

using namespace nv::index;
using namespace nv::index::xac;

class Volume_sample_program
{
    NV_IDX_VOLUME_SAMPLE_PROGRAM

    // sampling parameters
    const uint field_id         = 0u;
    const float dh              = 0.1f;

    // shading parameters
    const float diffuse_falloff = 1.0f;     // angular falloff for diffues lighting term
    const float shininess       = 100.0f;   // 'shininess' falloff parameter (lower is brighter)
    const float ambient_factor  = 0.1f;     // scaling of ambient term

    const float4 specular_color = make_float4(1.0f);

    const Custom_params*  m_custom_params;

public:
    NV_IDX_DEVICE_INLINE_MEMBER
    void initialize()
    {
        // maps Custom Visual Element GUI parameters to this kernel.
        m_custom_params = state.bind_parameter_buffer<Custom_params>(0);
    }

    NV_IDX_DEVICE_INLINE_MEMBER
    int execute(
        const Sample_info_self& sample_info,
              Sample_output&    sample_output) const
    {
        // map "pint 1" GUI parameter as use shading option (true/false)
        // enable/disable shading effect
        const bool use_shading = (m_custom_params->ints.x >= 1);

        // map "pfloat 1" GUI parameter as min shading alpha
        // min alpha threshold for shading.
        float min_shade_alpha = m_custom_params->floats.x/100.f;
        if(min_shade_alpha < 0.f)
          min_shade_alpha = 0.f;

        // sample volume
        const auto& cell_info = sample_info.sample_cell_info;
        float sample_value = sample_value = state.self.fetch_attribute<float>(field_id, cell_info);

        // get colormap
        const auto colormap = state.self.get_colormap();

        // lookup color value
        float4 sample_color = colormap.lookup(sample_value);

        // check if shading should be used
        if (use_shading && (sample_color.w > min_shade_alpha))
        {
            // make this sample full opaque
            sample_color.w = 1.f;

            // get the gradient (forward approximation, better performance)
            float3 gradient = get_gradient_3n(sample_info, state.self, sample_value);

            // use library function (central differences)
            //const float3 gradient = xaclib::volume_gradient(state.self, cell_info, dh);

            // get shading parameters
            const float3& view_direction = sample_info.ray_direction;
            const float3 iso_normal = -normalize(gradient);

            // apply built-in headlight shading
            sample_color = xaclib::headlight_shading(
                        state.scene,
                        iso_normal,
                        view_direction,
                        sample_color,
                        specular_color,
                        diffuse_falloff,
                        shininess,
                        ambient_factor);
        }

        // store the sample color & finish
        sample_output.set_color(sample_color);

        return NV_IDX_PROG_OK;
    }

    // compute gradient for irregular volume
    NV_IDX_DEVICE_INLINE_MEMBER
    float3 get_gradient_3n(const Sample_info_self&  sample_info, const Irregular_volume& ivol, const float vs_c) const
    {
        // get cell info
        const auto& cell_info = sample_info.sample_cell_info;

        const float vs_dx_p = ivol.fetch_attribute_offset<float>(field_id, cell_info, make_float3( dh, 0, 0));
        const float vs_dy_p = ivol.fetch_attribute_offset<float>(field_id, cell_info, make_float3(  0, dh, 0));
        const float vs_dz_p = ivol.fetch_attribute_offset<float>(field_id, cell_info, make_float3(  0, 0, dh));

        // get R3 gradient vector
        return make_float3(
            (vs_dx_p - vs_c) / dh,
            (vs_dy_p - vs_c) / dh,
            (vs_dz_p - vs_c) / dh);
    }
}; // class Volume_sample_program
