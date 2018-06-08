#include "stdafx.h"

#include <HLSLcc.h>

#include "Layers/xrRender/r_constants.h"

static class cl_sampler : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        CHK_GL(glProgramUniform1i(C->samp.program, C->samp.location, C->samp.index));
    }
} binder_sampler;

IC bool p_sort(ref_constant C1, ref_constant C2)
{
    return xr_strcmp(C1->name, C2->name) < 0;
}

BOOL R_constant_table::parseConstants(const ConstantBuffer* pTable, u32 destination)
{
    for (const auto& var : pTable->asVars)
    {
        auto name = var.name.c_str();

        u16 type = u16(-1);
        switch (var.sType.Type)
        {
        case SVT_FLOAT: type = RC_float; break;
        case SVT_BOOL: type = RC_bool; break;
        case SVT_INT: type = RC_int; break;
        default: fatal("R_constant_table::parse: unexpected shader variable type.");
        }

        VERIFY(var.sType.Offset < 0x10000);
        
        u16 r_index = u16(var.sType.Offset);
        u16 r_type = u16(-1);
        BOOL bSkip = FALSE;

        switch (var.sType.Class)
        {
        case SVC_SCALAR: r_type = RC_1x1; break;
        case SVC_VECTOR:
        {
            switch (var.sType.Columns)
            {
            case 4: r_type = RC_1x4; break;
            case 3: r_type = RC_1x3; break;
            case 2: r_type = RC_1x2; break;
            default: fatal("Vector: 1 components is scalar - there is special case for this!!!!!"); break;
            }
            break;
        }
        case SVC_MATRIX_ROWS:
        {
            switch (var.sType.Columns)
            {
            case 4:
                switch (var.sType.Rows)
                {
                case 2: r_type = RC_2x4; break;
                case 3:
                    r_type = RC_3x4;
                    break;
                    /*
                    switch (it->RegisterCount)
                    {
                    case 2: r_type  =   RC_2x4; break;
                    case 3: r_type  =   RC_3x4; break;
                    default:
                    fatal       ("MATRIX_ROWS: unsupported number of RegisterCount");
                    break;
                    }
                    break;
                    */
                case 4:
                    r_type = RC_4x4;
                    // VERIFY(4 == it->RegisterCount);
                    break;
                default: fatal("MATRIX_ROWS: unsupported number of Rows"); break;
                }
                break;
            default: fatal("MATRIX_ROWS: unsupported number of Columns"); break;
            }
            break;
        }
        case SVC_MATRIX_COLUMNS: fatal("Pclass MATRIX_COLUMNS unsupported"); break;
        case SVC_STRUCT: fatal("Pclass D3DXPC_STRUCT unsupported"); break;
        case SVC_OBJECT:
        {
            //  TODO: DX10:
            VERIFY(!"Implement shader object parsing.");
            /*
            switch (T->Type)
            {
            case D3DXPT_SAMPLER:
            case D3DXPT_SAMPLER1D:
            case D3DXPT_SAMPLER2D:
            case D3DXPT_SAMPLER3D:
            case D3DXPT_SAMPLERCUBE:
            {
            // ***Register sampler***
            // We have determined all valuable info, search if constant already created
            ref_constant    C       =   get (name);
            if (!C) {
            C                   =   new R_constant();//.g_constant_allocator.create();
            C->name             =   name;
            C->destination      =   RC_dest_sampler;
            C->type             =   RC_sampler;
            R_constant_load& L  =   C->samp;
            L.index             =   u16(r_index + ( (destination&1)? 0 : D3DVERTEXTEXTURESAMPLER0 ));
            L.cls               =   RC_sampler  ;
            table.push_back     (C);
            } else {
            R_ASSERT            (C->destination ==  RC_dest_sampler);
            R_ASSERT            (C->type        ==  RC_sampler);
            R_constant_load& L  =   C->samp;
            R_ASSERT            (L.index        ==  r_index);
            R_ASSERT            (L.cls          ==  RC_sampler);
            }
            }
            break;
            default:
            fatal       ("Pclass D3DXPC_OBJECT - object isn't of 'sampler' type");
            break;
            }
            */
            bSkip = TRUE;
            break;
        }
        default: bSkip = TRUE; break;
        }

        if (bSkip)
            continue;

        // We have determined all valuable info, search if constant already created
        ref_constant C = get(name);
        if (!C)
        {
            C = new R_constant(); //.g_constant_allocator.create();
            C->name = name;
            C->destination = destination;
            C->type = type;
            // R_constant_load& L   =   (destination&1)?C->ps:C->vs;
            R_constant_load& L = C->get_load(destination); /*((destination&RC_dest_pixel)
                                                           ? C->ps : (destination&RC_dest_vertex)
                                                           ? C->vs : C->gs);*/
            L.index = r_index;
            L.cls = r_type;
            table.push_back(C);
        }
        else
        {
            C->destination |= destination;
            VERIFY(C->type == type);
            // R_constant_load& L   =   (destination&1)?C->ps:C->vs;
            R_constant_load& L = C->get_load(destination); /*((destination&RC_dest_pixel)
                                                           ? C->ps : (destination&RC_dest_vertex)
                                                           ? C->vs : C->gs);*/
            L.index = r_index;
            L.cls = r_type;
        }
    }
    return TRUE;
}

BOOL R_constant_table::parseResources(const ShaderInfo* shaderInfo, u32 destination)
{
    for (const auto& binding : shaderInfo->psResourceBindings)
    {
        //D3D_SHADER_INPUT_BIND_DESC ResDesc;
        //pReflection->GetResourceBindingDesc(i, &ResDesc);

        u16 type = 0;

        switch (binding.eType)
        {
        case RTYPE_TEXTURE: type = RC_dx10texture; break;
        case RTYPE_SAMPLER: type = RC_sampler; break;
        case RTYPE_UAV_RWTYPED: type = RC_dx11UAV; break;
        default: continue;
        }

        VERIFY(binding.ui32BindCount == 1);

        // u16  r_index = u16( ResDesc.BindPoint + ((destination&1)? 0 : CTexture::rstVertex) );

        u16 r_index = u16(-1);

        if (destination & RC_dest_pixel)
            r_index = u16(binding.ui32BindPoint + CTexture::rstPixel);

        else if (destination & RC_dest_vertex)
            r_index = u16(binding.ui32BindPoint + CTexture::rstVertex);

        else if (destination & RC_dest_geometry)
            r_index = u16(binding.ui32BindPoint + CTexture::rstGeometry);

        /*else if (destination & RC_dest_hull)
            r_index = u16(binding.ui32BindPoint + CTexture::rstHull);

        else if (destination & RC_dest_domain)
            r_index = u16(binding.ui32BindPoint + CTexture::rstDomain);

        else if (destination & RC_dest_compute)
            r_index = u16(binding.ui32BindPoint + CTexture::rstCompute);*/

        else
            VERIFY(0);

        const auto name = binding.name.c_str();

        ref_constant C = get(name);
        if (!C)
        {
            C = new R_constant(); //.g_constant_allocator.create();
            C->name = name;
            C->destination = RC_dest_sampler;
            C->type = type;
            R_constant_load& L = C->samp;
            L.index = r_index;
            L.cls = type;
            table.push_back(C);
        }
        else
        {
            R_ASSERT(C->destination == RC_dest_sampler);
            R_ASSERT(C->type == type);
            R_constant_load& L = C->samp;
            R_ASSERT(L.index == r_index);
            R_ASSERT(L.cls == type);
        }
    }
    return TRUE;
}

IC u32 dest_to_shift_value(u32 destination)
{
    switch (destination & 0xFF)
    {
    case RC_dest_vertex: return RC_dest_vertex_cb_index_shift;
    case RC_dest_pixel: return RC_dest_pixel_cb_index_shift;
#if defined(USE_DX10) || defined(USE_DX11)
    case RC_dest_geometry: return RC_dest_geometry_cb_index_shift;
#ifdef USE_DX11
    case RC_dest_hull: return RC_dest_hull_cb_index_shift;
    case RC_dest_domain: return RC_dest_domain_cb_index_shift;
    case RC_dest_compute: return RC_dest_compute_cb_index_shift;
#endif
#endif
    default: FATAL("invalid enumeration for shader");
    }
    return 0;
}

IC u32 dest_to_cbuf_type(u32 destination)
{
    switch (destination & 0xFF)
    {
    case RC_dest_vertex: return CB_BufferVertexShader;
    case RC_dest_pixel: return CB_BufferPixelShader;
#if defined(USE_DX10) || defined(USE_DX11)
    case RC_dest_geometry: return CB_BufferGeometryShader;
#ifdef USE_DX11
    case RC_dest_hull: return CB_BufferHullShader;
    case RC_dest_domain: return CB_BufferDomainShader;
    case RC_dest_compute: return CB_BufferComputeShader;
#endif
#endif
    default: FATAL("invalid enumeration for shader");
    }
    return 0;
}

BOOL R_constant_table::parse(const ShaderInfo* shaderInfo, u32 destination)
{
    for (const auto& cbuffer : shaderInfo->psConstantBuffers)
        parseConstants(&cbuffer, destination);

    if (!shaderInfo->psResourceBindings.empty())
        parseResources(shaderInfo, destination);

    std::sort(table.begin(), table.end(), p_sort);
    return TRUE;
}

// TODO: OGL: Use constant buffers like DX10.
BOOL R_constant_table::parse(void* _desc, u32 destination)
{
    GLuint program = *(GLuint*)_desc;

    // Get the maximum length of the constant name and allocate a buffer for it
    GLint maxLength;
    CHK_GL(glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength));
    GLchar* name = xr_alloc<GLchar>(maxLength + 1); // Null terminator

    // Iterate all uniforms and parse the entries for the constant table.
    GLint uniformCount;
    CHK_GL(glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount));

    u16 r_stage = 0;
    for (GLint i = 0; i < uniformCount; i++)
    {
        GLint size;
        GLenum reg;
        CHK_GL(glGetActiveUniform(program, i, maxLength, NULL, &size, &reg, name));

        // Remove index from arrays
        if (size > 1)
        {
            char* str = strstr(name, "[0]");
            if (str) *str = '\0';
        }

        u16 type = RC_float;
        if (GL_BOOL == reg ||
            GL_BOOL_VEC2 == reg ||
            GL_BOOL_VEC3 == reg ||
            GL_BOOL_VEC4 == reg)
            type = RC_bool;
        if (GL_INT == reg ||
            GL_INT_VEC2 == reg ||
            GL_INT_VEC3 == reg ||
            GL_INT_VEC4 == reg)
            type = RC_int;

        // Rindex,Rcount,Rlocation
        u16 r_index = i;
        u16 r_type = u16(-1);
        GLuint r_location = glGetUniformLocation(program, name);

        // TypeInfo + class
        BOOL bSkip = FALSE;
        switch (reg)
        {
        case GL_FLOAT:
        case GL_BOOL:
        case GL_INT:
            r_type = RC_1x1;
            break;
        case GL_FLOAT_VEC2:
        case GL_BOOL_VEC2:
        case GL_INT_VEC2:
            r_type = RC_1x2;
            break;
        case GL_FLOAT_VEC3:
        case GL_BOOL_VEC3:
        case GL_INT_VEC3:
            r_type = RC_1x3;
            break;
        case GL_FLOAT_VEC4:
        case GL_BOOL_VEC4:
        case GL_INT_VEC4:
            r_type = RC_1x4;
            break;
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
            fatal("GL_FLOAT_MAT: unsupported number of dimensions");
            break;
        case GL_FLOAT_MAT4x2:
            r_type = RC_2x4;
            break;
        case GL_FLOAT_MAT4x3:
            r_type = RC_3x4;
            break;
        case GL_FLOAT_MAT4:
            r_type = RC_4x4;
            break;
        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_MULTISAMPLE:
        {
            // ***Register sampler***
            // We have determined all valuable info, search if constant already created
            // Assign an unused stage as the index
            ref_constant C = get(name);
            if (!C)
            {
                VERIFY2(false, name);
                /*C = new R_constant();//.g_constant_allocator.create();
                C->name = name;
                C->destination = RC_dest_sampler;
                C->type = RC_sampler;
                C->handler = &binder_sampler;
                R_constant_load& L = C->samp;
                L.index = r_stage++;
                L.cls = RC_sampler;
                L.location = r_location;
                L.program = program;
                table.push_back(C);*/
            }
            else
            {
                R_ASSERT(C->destination == RC_dest_sampler);
                R_ASSERT(C->type == RC_sampler);
                R_ASSERT(C->handler == &binder_sampler);
                R_constant_load& L = C->samp;
                R_ASSERT(L.index == r_stage);
                R_ASSERT(L.cls == RC_sampler);
                L.location = r_location;
                L.program = program;
            }
        }
            bSkip = TRUE;
            break;
        default:
            fatal("unsupported uniform");
            bSkip = TRUE;
            break;
        }
        if (bSkip) continue;

        // We have determined all valuable info, search if constant already created
        ref_constant C = get(name);
        if (!C)
        {
            VERIFY2(false, name);
            /*C = new R_constant();//.g_constant_allocator.create();
            C->name = name;
            C->destination = destination;
            C->type = type;
            R_constant_load& L = destination & 1 ? C->ps : C->vs;
            L.index = r_index;
            L.cls = r_type;
            L.location = r_location;
            L.program = program;
            table.push_back(C);*/
        }
        else
        {
            //C->destination |= destination;
            R_ASSERT(C->destination == destination);
            VERIFY (C->type == type);
            R_constant_load& L = destination & 1 ? C->ps : C->vs;
            R_ASSERT(L.index == r_index);
            R_ASSERT(L.cls == r_type);
            L.location = r_location;
            L.program = program;
        }
    }
    sort(table.begin(), table.end(), p_sort);
    xr_free(name);
    return TRUE;
}
