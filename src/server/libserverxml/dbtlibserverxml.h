/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LIBRARY_NAME "libserverxml"

static char dbtlibserverxmlid[] = "$DBT: libserverxml$";

#include "i18n.h"

/* Message IDs reserved for this file: CONF1100-CONF1199 */
BEGIN_STR(libserverxml)
    ResDef(DBT_LibraryID_, -1, dbtlibserverxmlid)
    ResDef(DBT_CONF1101_unexpected_error, 1, "CONF1101: Unexpected error")
    ResDef(DBT_CONF1102_invalid_syntax, 2, "CONF1102: Invalid syntax")
    ResDef(DBT_CONF1103_xerces_error_prefix, 3, "CONF1103: Error from XML parser: ")
    ResDef(DBT_CONF1104_invalid_tag_X_value_nY, 4, "CONF1104: Invalid <%s> value: %.*s")
    ResDef(DBT_CONF1105_invalid_tag_X_value, 5, "CONF1105: Invalid <%s> value")
    ResDef(DBT_CONF1106_tag_X_must_not_be_empty, 6, "CONF1106: Missing required <%s> value")
    ResDef(DBT_CONF1107_tag_X_missing, 7, "CONF1107: Missing required <%s> element")
    ResDef(DBT_CONF1108_tag_X_previously_defined_on_line_Y, 8, "CONF1108: <%s> was previously defined on line %d")
    ResDef(DBT_CONF1109_tag_X_value_Y_duplicates_line_Z, 9, "CONF1109: <%s> \"%s\" was previously defined on line %d")
    ResDef(DBT_CONF1110_selector_X_field_Y_key_Z_undefined, 10, "CONF1110: No <%s> with <%s> \"%s\" was defined")
    ResDef(DBT_filename_X_lines_Y_Z_prefix, 11, "File %s lines %d-%d: ")
    ResDef(DBT_filename_X_line_Y_prefix, 12, "File %s line %d: ")
    ResDef(DBT_filename_X_prefix, 13, "File %s: ")
    ResDef(DBT_tag_X_prefix, 14, "Element <%s>: ")
    ResDef(DBT_CONF1115_error_opening_X_error_Y, 15, "CONF1115: Error opening %s (%s)")
    ResDef(DBT_CONF1116_expected_server, 16, "CONF1116: Expected <server>")
    ResDef(DBT_CONF1117_unknown_element, 17, "CONF1117: Unknown or unexpected element")
END_STR(base)
