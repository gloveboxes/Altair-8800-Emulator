/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_async.h"
#include <stddef.h>
#include <stdint.h>

size_t openai_output(int port, uint8_t data, char *buffer, size_t buffer_length);
uint8_t openai_input(uint8_t port);
void init_openai(const char *openai_api_key, const char *openai_endpoint);