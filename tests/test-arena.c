/*
Copyright (c) 2020. The YARA Authors. All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <yara/arena2.h>
#include <yara/stream.h>
#include <yara.h>

static void basic_tests()
{
  YR_ARENA2* arena;

  // Create arena with 1 buffer of 10 bytes of initial size.
  assert(yr_arena2_create(1, 10, &arena) == ERROR_SUCCESS);

  yr_arena_off_t offset;

  // Allocate 5 bytes.
  assert(yr_arena2_allocate_memory(arena, 0, 5, &offset) == ERROR_SUCCESS);

  // Offset should be 0 as this is the first write.
  assert(offset == 0);

  // Write 4 bytes, "foo" + null terminator.
  assert(yr_arena2_write_string(arena, 0, "foo", &offset) == ERROR_SUCCESS);

  // Offset should be 5 as this was written after the first 5-bytes write.
  assert(offset == 5);

  // Write 4 bytes, "bar" + null terminator. This makes forces a reallocation.
  assert(yr_arena2_write_string(arena, 0, "bar", &offset) == ERROR_SUCCESS);

  // Offset should be 9.
  assert(offset == 9);

  yr_arena2_destroy(arena);
}


typedef struct TEST_STRUCT TEST_STRUCT;

struct TEST_STRUCT {
  char* str1;
  char* str2;
};


static void reloc_tests()
{
  YR_ARENA2* arena;

  // Create arena with 2 buffers of 10 bytes of initial size.
  int result = yr_arena2_create(2, 10, &arena);
  assert(result == ERROR_SUCCESS);

  yr_arena_off_t offset;

  // Allocate a struct in buffer 0 indicating that the field "str" is a
  // relocatable pointer.
  result = yr_arena2_allocate_struct(
      arena,
      0,
      sizeof(TEST_STRUCT),
      &offset,
      offsetof(TEST_STRUCT, str1),
      offsetof(TEST_STRUCT, str2),
      EOL2);

  assert(result == ERROR_SUCCESS);

  // Get the struct address, this pointer is valid as longs as we don't call
  // any other function that allocates memory in buffer 0.
  TEST_STRUCT* s = (TEST_STRUCT*) yr_arena2_get_address(arena, 0, offset);

  // Write a string in buffer 1.
  yr_arena2_write_string(arena, 1, "foo", &offset);

  // Get the string's address and store it in the struct's "str" field.
  s->str1 = (char *) yr_arena2_get_address(arena, 1, offset);

  // Write another string in buffer 1.
  yr_arena2_write_string(arena, 1, "bar", &offset);

  // Get the string's address and store it in the struct's "str" field.
  s->str2 = (char *) yr_arena2_get_address(arena, 1, offset);

  // The arena should have two reloc entries for the "str1" and "str2" fields.
  assert(arena->reloc_list_head != NULL);
  assert(arena->reloc_list_tail != NULL);
  assert(arena->reloc_list_head->buffer_id == 0);
  assert(arena->reloc_list_tail->buffer_id == 0);
  assert(arena->reloc_list_head->offset == offsetof(TEST_STRUCT, str1));
  assert(arena->reloc_list_tail->offset == offsetof(TEST_STRUCT, str2));

  // Write another string in buffer 1 that causes a buffer reallocation.
  yr_arena2_write_string(arena, 1, "aaaaaaaaaaa", &offset);

  assert(strcmp(s->str1, "foo") == 0);
  assert(strcmp(s->str2, "bar") == 0);

  YR_STREAM stream;
  FILE* fh = fopen("test-arena-stream", "w+");

  assert(fh != NULL);

  stream.user_data = fh;
  stream.write = (YR_STREAM_WRITE_FUNC) fwrite;
  stream.read = (YR_STREAM_READ_FUNC) fread;

  if (yr_arena2_save_stream(arena, &stream) != ERROR_SUCCESS)
    exit(EXIT_FAILURE);

  fflush(fh);
  fseek(fh, 0, SEEK_SET);

  assert(strcmp(s->str1, "foo") == 0);
  assert(strcmp(s->str2, "bar") == 0);

  yr_arena2_destroy(arena);

  result = yr_arena2_load_stream(&stream, &arena);
  assert(result == ERROR_SUCCESS);

  s = (TEST_STRUCT*) yr_arena2_get_address(arena, 0, 0);

  assert(strcmp(s->str1, "foo") == 0);
  assert(strcmp(s->str2, "bar") == 0);

  fclose(fh);
  yr_arena2_destroy(arena);
}

int main(int argc, char** argv)
{
  //basic_tests();
  reloc_tests();

  return 0;
}
