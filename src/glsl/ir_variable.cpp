/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ir.h"
#include "glsl_parser_extras.h"
#include "glsl_symbol_table.h"
#include "builtin_variables.h"

#ifndef Elements
#define Elements(x) (sizeof(x)/sizeof(*(x)))
#endif

static void generate_ARB_draw_buffers_variables(exec_list *,
						struct _mesa_glsl_parse_state *,
						bool, _mesa_glsl_parser_targets);

static ir_variable *
add_variable(const char *name, enum ir_variable_mode mode, int slot,
	     const glsl_type *type, exec_list *instructions,
		     glsl_symbol_table *symtab)
{
   ir_variable *var = new(symtab) ir_variable(type, name);

   var->mode = mode;
   switch (var->mode) {
   case ir_var_auto:
      var->read_only = true;
      break;
   case ir_var_in:
      var->shader_in = true;
      var->read_only = true;
      break;
   case ir_var_inout:
      var->shader_in = true;
      var->shader_out = true;
      break;
   case ir_var_out:
      var->shader_out = true;
      break;
   case ir_var_uniform:
      var->shader_in = true;
      var->read_only = true;
      break;
   default:
      assert(0);
      break;
   }

   var->location = slot;

   /* Once the variable is created an initialized, add it to the symbol table
    * and add the declaration to the IR stream.
    */
   instructions->push_tail(var);

   symtab->add_variable(var->name, var);
   return var;
}


static void
add_builtin_variable(const builtin_variable *proto, exec_list *instructions,
		     glsl_symbol_table *symtab)
{
   /* Create a new variable declaration from the description supplied by
    * the caller.
    */
   const glsl_type *const type = symtab->get_type(proto->type);

   assert(type != NULL);

   add_variable(proto->name, proto->mode, proto->slot, type, instructions,
		symtab);
}


static void
generate_110_uniforms(exec_list *instructions,
		      struct _mesa_glsl_parse_state *state)
{
   for (unsigned i = 0
	   ; i < Elements(builtin_110_deprecated_uniforms)
	   ; i++) {
      add_builtin_variable(& builtin_110_deprecated_uniforms[i],
			   instructions, state->symbols);
   }

   ir_variable *const mtc = add_variable("gl_MaxTextureCoords", ir_var_auto,
					 -1, glsl_type::int_type,
					 instructions, state->symbols);
   mtc->constant_value = new(mtc)
      ir_constant(int(state->Const.MaxTextureCoords));


   const glsl_type *const mat4_array_type =
      glsl_type::get_array_instance(state->symbols, glsl_type::mat4_type,
				    state->Const.MaxTextureCoords);

   add_variable("gl_TextureMatrix", ir_var_uniform, -1, mat4_array_type,
		instructions, state->symbols);

   /* FINISHME: Add support for gl_DepthRangeParameters */
   /* FINISHME: Add support for gl_ClipPlane[] */
   /* FINISHME: Add support for gl_PointParameters */

   /* FINISHME: Add support for gl_MaterialParameters
    * FINISHME: (glFrontMaterial, glBackMaterial)
    */

   /* FINISHME: The size of this array is implementation dependent based on the
    * FINISHME: value of GL_MAX_TEXTURE_LIGHTS.  GL_MAX_TEXTURE_LIGHTS must be
    * FINISHME: at least 8, so hard-code 8 for now.
    */
   const glsl_type *const light_source_array_type =
      glsl_type::get_array_instance(state->symbols,
				    state->symbols->get_type("gl_LightSourceParameters"), 8);

   add_variable("gl_LightSource", ir_var_uniform, -1, light_source_array_type,
		instructions, state->symbols);

   /* FINISHME: Add support for gl_LightModel */
   /* FINISHME: Add support for gl_FrontLightProduct[], gl_BackLightProduct[] */
   /* FINISHME: Add support for gl_TextureEnvColor[] */
   /* FINISHME: Add support for gl_ObjectPlane*[], gl_EyePlane*[] */
   /* FINISHME: Add support for gl_Fog */
}

static void
generate_110_vs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   for (unsigned i = 0; i < Elements(builtin_core_vs_variables); i++) {
      add_builtin_variable(& builtin_core_vs_variables[i],
			   instructions, state->symbols);
   }

   for (unsigned i = 0
	   ; i < Elements(builtin_110_deprecated_vs_variables)
	   ; i++) {
      add_builtin_variable(& builtin_110_deprecated_vs_variables[i],
			   instructions, state->symbols);
   }
   generate_110_uniforms(instructions, state);

   /* From page 54 (page 60 of the PDF) of the GLSL 1.20 spec:
    *
    *     "As with all arrays, indices used to subscript gl_TexCoord must
    *     either be an integral constant expressions, or this array must be
    *     re-declared by the shader with a size. The size can be at most
    *     gl_MaxTextureCoords. Using indexes close to 0 may aid the
    *     implementation in preserving varying resources."
    */
   const glsl_type *const vec4_array_type =
      glsl_type::get_array_instance(state->symbols, glsl_type::vec4_type, 0);

   add_variable("gl_TexCoord", ir_var_out, VERT_RESULT_TEX0, vec4_array_type,
		instructions, state->symbols);

   generate_ARB_draw_buffers_variables(instructions, state, false,
				       vertex_shader);
}


static void
generate_120_vs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   /* GLSL version 1.20 did not add any built-in variables in the vertex
    * shader.
    */
   generate_110_vs_variables(instructions, state);
}


static void
generate_130_vs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   void *ctx = state->symbols;
   generate_120_vs_variables(instructions, state);

   for (unsigned i = 0; i < Elements(builtin_130_vs_variables); i++) {
      add_builtin_variable(& builtin_130_vs_variables[i],
			   instructions, state->symbols);
   }

   /* FINISHME: The size of this array is implementation dependent based on
    * FINISHME: the value of GL_MAX_CLIP_DISTANCES.
    */
   const glsl_type *const clip_distance_array_type =
      glsl_type::get_array_instance(ctx, glsl_type::float_type, 8);

   /* FINISHME: gl_ClipDistance needs a real location assigned. */
   add_variable("gl_ClipDistance", ir_var_out, -1, clip_distance_array_type,
		instructions, state->symbols);

}


static void
initialize_vs_variables(exec_list *instructions,
			struct _mesa_glsl_parse_state *state)
{

   switch (state->language_version) {
   case 110:
      generate_110_vs_variables(instructions, state);
      break;
   case 120:
      generate_120_vs_variables(instructions, state);
      break;
   case 130:
      generate_130_vs_variables(instructions, state);
      break;
   }
}

static void
generate_110_fs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   for (unsigned i = 0; i < Elements(builtin_core_fs_variables); i++) {
      add_builtin_variable(& builtin_core_fs_variables[i],
			   instructions, state->symbols);
   }

   for (unsigned i = 0
	   ; i < Elements(builtin_110_deprecated_fs_variables)
	   ; i++) {
      add_builtin_variable(& builtin_110_deprecated_fs_variables[i],
			   instructions, state->symbols);
   }
   generate_110_uniforms(instructions, state);

   /* From page 54 (page 60 of the PDF) of the GLSL 1.20 spec:
    *
    *     "As with all arrays, indices used to subscript gl_TexCoord must
    *     either be an integral constant expressions, or this array must be
    *     re-declared by the shader with a size. The size can be at most
    *     gl_MaxTextureCoords. Using indexes close to 0 may aid the
    *     implementation in preserving varying resources."
    */
   const glsl_type *const vec4_array_type =
      glsl_type::get_array_instance(state->symbols, glsl_type::vec4_type, 0);

   add_variable("gl_TexCoord", ir_var_in, FRAG_ATTRIB_TEX0, vec4_array_type,
		instructions, state->symbols);

   generate_ARB_draw_buffers_variables(instructions, state, false,
				       fragment_shader);
}


static void
generate_ARB_draw_buffers_variables(exec_list *instructions,
				    struct _mesa_glsl_parse_state *state,
				    bool warn, _mesa_glsl_parser_targets target)
{
   /* gl_MaxDrawBuffers is available in all shader stages.
    */
   ir_variable *const mdb =
      add_variable("gl_MaxDrawBuffers", ir_var_auto, -1,
		   glsl_type::int_type, instructions, state->symbols);

   if (warn)
      mdb->warn_extension = "GL_ARB_draw_buffers";

   mdb->constant_value = new(mdb)
      ir_constant(int(state->Const.MaxDrawBuffers));


   /* gl_FragData is only available in the fragment shader.
    */
   if (target == fragment_shader) {
      const glsl_type *const vec4_array_type =
	 glsl_type::get_array_instance(state->symbols, glsl_type::vec4_type,
				       state->Const.MaxDrawBuffers);

      ir_variable *const fd =
	 add_variable("gl_FragData", ir_var_out, FRAG_RESULT_DATA0,
		      vec4_array_type, instructions, state->symbols);

      if (warn)
	 fd->warn_extension = "GL_ARB_draw_buffers";
   }
}


static void
generate_120_fs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   generate_110_fs_variables(instructions, state);

   for (unsigned i = 0
	   ; i < Elements(builtin_120_fs_variables)
	   ; i++) {
      add_builtin_variable(& builtin_120_fs_variables[i],
			   instructions, state->symbols);
   }
}

static void
generate_130_fs_variables(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state)
{
   void *ctx = state->symbols;
   generate_120_fs_variables(instructions, state);

   /* FINISHME: The size of this array is implementation dependent based on
    * FINISHME: the value of GL_MAX_CLIP_DISTANCES.
    */
   const glsl_type *const clip_distance_array_type =
      glsl_type::get_array_instance(ctx, glsl_type::float_type, 8);

   /* FINISHME: gl_ClipDistance needs a real location assigned. */
   add_variable("gl_ClipDistance", ir_var_in, -1, clip_distance_array_type,
		instructions, state->symbols);
}

static void
initialize_fs_variables(exec_list *instructions,
			struct _mesa_glsl_parse_state *state)
{

   switch (state->language_version) {
   case 110:
      generate_110_fs_variables(instructions, state);
      break;
   case 120:
      generate_120_fs_variables(instructions, state);
      break;
   case 130:
      generate_130_fs_variables(instructions, state);
      break;
   }
}

void
_mesa_glsl_initialize_variables(exec_list *instructions,
				struct _mesa_glsl_parse_state *state)
{
   switch (state->target) {
   case vertex_shader:
      initialize_vs_variables(instructions, state);
      break;
   case geometry_shader:
      break;
   case fragment_shader:
      initialize_fs_variables(instructions, state);
      break;
   case ir_shader:
      fprintf(stderr, "ir reader has no builtin variables");
      exit(1);
      break;
   }
}