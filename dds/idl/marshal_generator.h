/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef marshal_generator_H
#define marshal_generator_H

#include "dds_generator.h"

class marshal_generator : public dds_generator {
public:
  bool gen_enum(AST_Enum* node, UTL_ScopedName* name,
                const std::vector<AST_EnumVal*>& contents, const char* repoid);

  bool gen_struct(AST_Structure* node, UTL_ScopedName* name,
                  const std::vector<AST_Field*>& fields,
                  AST_Type::SIZE_TYPE size, const char* repoid);

  bool gen_typedef(AST_Typedef* node, UTL_ScopedName* name, AST_Type* base, const char* repoid);

  bool gen_union(AST_Union* node, UTL_ScopedName* name,
                 const std::vector<AST_UnionBranch*>& branches,
                 AST_Type* discriminator,
                 const char* repoid);

  static bool generate_struct_deserialization(AST_Structure* node, FieldFilter field_type);

  static void clayton_gen_field_getValueFromSerialized(AST_Structure* node, const std::string& clazz);
};

#endif
