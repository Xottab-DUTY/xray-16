#include "stdafx.h"
#pragma hdrstop

#include <glslang/glslang/Include/Types.h>
#include "../xrRender/r_constants.h"

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

BOOL R_constant_table::parseConstants(const glslang::TType* pTable, u32 destination)
{
    const glslang::TTypeList* TableDesc = pTable->getStruct();

    for (u32 i = 0; i < TableDesc->size(); ++i)
    {
        const glslang::TType* pType = TableDesc->at(i).type;
        VERIFY(pType);

        // Name
        // LPCSTR   name        =   LPCSTR(ptr+it->Name);
        LPCSTR name = pType->getFieldName().c_str();

        // Type
        // u16      type        =   RC_float;
        u16 type = u16(-1);
        switch (pType->getBasicType())
        {
        case glslang::EbtFloat: type = RC_float; break;
        case glslang::EbtBool: type = RC_bool; break;
        case glslang::EbtInt: type = RC_int; break;
        default: fatal("R_constant_table::parse: unexpected shader variable type.");
        }

        // Rindex,Rcount
        // u16      r_index     =   it->RegisterIndex;
        //  Used as byte offset in constant buffer
        VERIFY(pType->getQualifier().layoutOffset < 0x10000);
        u16 r_index = u16(pType->getQualifier().layoutOffset);
        u16 r_type = u16(-1);

        // TypeInfo + class
        // D3DXSHADER_TYPEINFO* T   = (D3DXSHADER_TYPEINFO*)(ptr+it->TypeInfo);
        BOOL bSkip = FALSE;
        // switch (T->Class)

        if (pType->isScalarOrVec1())
        {
            switch (pType->getVectorSize())
            {
            case 4: r_type = RC_1x4; break;
            case 3: r_type = RC_1x3; break;
            case 2: r_type = RC_1x2; break;
            case 1: r_type = RC_1x1; break;
            default: fatal("R_constant_table::parse: Unsupported vector size."); break;
            }
        }
        else if (pType->isMatrix())
        {
            switch (pType->getMatrixCols())
            {
            case 4:
                switch (pType->getMatrixRows())
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
        }
        else
        {
            bSkip = TRUE;
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

BOOL R_constant_table::parseResources(const glslang::TProgram* pReflection, u32 destination)
{
    for (int i = 0; i < pReflection->getNumLiveUniformVariables(); ++i)
    {
        const glslang::TType* pType = pReflection->getUniformTType(i);
        if (!pType->isOpaque())
            continue;

        const glslang::TSampler& sampler = pType->getSampler();

        LPCSTR name = pReflection->getUniformName(i);

        u16 type = 0;
        if (sampler.isTexture())
            type = RC_dx10texture;
        else if (sampler.isPureSampler())
            type = RC_sampler;
        else if (sampler.isImage())
            type = RC_dx11UAV;

        // VERIFY(ResDesc.BindCount == 1);

        // u16  r_index = u16( ResDesc.BindPoint + ((destination&1)? 0 : CTexture::rstVertex) );

        u16 r_index = u16(-1);

        if (destination & RC_dest_pixel)
        {
            r_index = u16(i + CTexture::rstPixel);
        }
        else if (destination & RC_dest_vertex)
        {
            r_index = u16(i + CTexture::rstVertex);
        }
        else if (destination & RC_dest_geometry)
        {
            r_index = u16(i + CTexture::rstGeometry);
        }
        // XXX: Enable this!
        /*else if (destination & RC_dest_hull)
        {
            r_index = u16(i + CTexture::rstHull);
        }
        else if (destination & RC_dest_domain)
        {
            r_index = u16(i + CTexture::rstDomain);
        }
        else if (destination & RC_dest_compute)
        {
            r_index = u16(i + CTexture::rstCompute);
        }*/
        else
        {
            VERIFY(0);
        }

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
#if defined(USE_DX10) || defined(USE_DX11) || defined(USE_OGL)
    case RC_dest_geometry: return RC_dest_geometry_cb_index_shift;
#if defined(USE_DX11) || defined(USE_OGL)
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
#if defined(USE_DX10) || defined(USE_DX11) || defined(USE_OGL)
    case RC_dest_geometry: return CB_BufferGeometryShader;
#if defined(USE_DX11) || defined(USE_OGL)
    case RC_dest_hull: return CB_BufferHullShader;
    case RC_dest_domain: return CB_BufferDomainShader;
    case RC_dest_compute: return CB_BufferComputeShader;
#endif
#endif
    default: FATAL("invalid enumeration for shader");
    }
    return 0;
}

// TODO: OGL: Use constant buffers like DX10.
BOOL R_constant_table::parse(void* _desc, u32 destination)
{
    glslang::TProgram* pReflection = (glslang::TProgram*)_desc;

    if (pReflection->getNumLiveUniformBlocks() > 0)
    {
        //m_CBTable.reserve(pReflection->getNumLiveUniformBlocks());
        //  Parse single constant table
        const glslang::TType* pTable = 0;

        for (u16 iBuf = 0; iBuf < pReflection->getNumLiveUniformBlocks(); ++iBuf)
        {
            pTable = pReflection->getUniformBlockTType(iBuf);
            if (pTable)
            {
                //  Encode buffer index into destination
                u32 updatedDest = destination;
                updatedDest |= iBuf << dest_to_shift_value(destination); /*((destination&RC_dest_pixel)
                                                                         ? RC_dest_pixel_cb_index_shift : (destination&RC_dest_vertex)
                                                                         ? RC_dest_vertex_cb_index_shift : RC_dest_geometry_cb_index_shift);*/

                                                                         //  Encode bind dest (pixel/vertex buffer) and bind point index
                u32 uiBufferIndex = iBuf;
                uiBufferIndex |= dest_to_cbuf_type(destination); /*(destination&RC_dest_pixel)
                                                                 ? CB_BufferPixelShader : (destination&RC_dest_vertex)
                                                                 ? CB_BufferVertexShader : CB_BufferGeometryShader;*/

                parseConstants(pTable, updatedDest);
                //ref_cbuffer tempBuffer = RImplementation.Resources->_CreateConstantBuffer(pTable, pReflection->getUniformBlockSize(iBuf));
                //m_CBTable.push_back(cb_table_record(uiBufferIndex, tempBuffer));
            }
        }
    }

    parseResources(pReflection, destination);

    std::sort(table.begin(), table.end(), p_sort);
    return TRUE;

#if 0
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
                C = new R_constant();//.g_constant_allocator.create();
                C->name = name;
                C->destination = RC_dest_sampler;
                C->type = RC_sampler;
                C->handler = &binder_sampler;
                R_constant_load& L = C->samp;
                L.index = r_stage++;
                L.cls = RC_sampler;
                L.location = r_location;
                L.program = program;
                table.push_back(C);
            }
            else
            {
                R_ASSERT(C->destination == RC_dest_sampler);
                R_ASSERT(C->type == RC_sampler);
                R_ASSERT(C->handler == &binder_sampler);
                R_constant_load& L = C->samp;
                R_ASSERT(L.index == r_stage);
                R_ASSERT(L.cls == RC_sampler);
                R_ASSERT(L.location == r_location);
                R_ASSERT(L.program == program);
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
            C = new R_constant();//.g_constant_allocator.create();
            C->name = name;
            C->destination = destination;
            C->type = type;
            R_constant_load& L = destination & 1 ? C->ps : C->vs;
            L.index = r_index;
            L.cls = r_type;
            L.location = r_location;
            L.program = program;
            table.push_back(C);
        }
        else
        {
            C->destination |= destination;
            VERIFY (C->type == type);
            R_constant_load& L = destination & 1 ? C->ps : C->vs;
            L.index = r_index;
            L.cls = r_type;
            L.location = r_location;
            L.program = program;
        }
    }
    sort(table.begin(), table.end(), p_sort);
    xr_free(name);
    return TRUE;
#endif
}
