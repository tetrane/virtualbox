/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * CBC self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include "cbc_test.h"

/**
 * Test CBC encryption
 *
 * @v cipher			Cipher algorithm
 * @v key			Key
 * @v key_len			Length of key
 * @v iv			Initialisation vector
 * @v plaintext			Plaintext data
 * @v expected_ciphertext	Expected ciphertext data
 * @v len			Length of data
 * @ret ok			Ciphertext is as expected
 */
int cbc_test_encrypt ( struct cipher_algorithm *cipher, const void *key,
		       size_t key_len, const void *iv, const void *plaintext,
		       const void *expected_ciphertext, size_t len ) {
	uint8_t ctx[ cipher->ctxsize ];
	uint8_t ciphertext[ len ];
	int rc;

	/* Initialise cipher */
	rc = cipher_setkey ( cipher, ctx, key, key_len );
	assert ( rc == 0 );
	cipher_setiv ( cipher, ctx, iv );

	/* Perform encryption */
	cipher_encrypt ( cipher, ctx, plaintext, ciphertext, len );

	/* Verify result */
	return ( memcmp ( ciphertext, expected_ciphertext, len ) == 0 );
}

/**
 * Test CBC decryption
 *
 * @v cipher			Cipher algorithm
 * @v key			Key
 * @v key_len			Length of key
 * @v iv			Initialisation vector
 * @v ciphertext		Ciphertext data
 * @v expected_plaintext	Expected plaintext data
 * @v len			Length of data
 * @ret ok			Plaintext is as expected
 */
int cbc_test_decrypt ( struct cipher_algorithm *cipher, const void *key,
		       size_t key_len, const void *iv, const void *ciphertext,
		       const void *expected_plaintext, size_t len ) {
	uint8_t ctx[ cipher->ctxsize ];
	uint8_t plaintext[ len ];
	int rc;

	/* Initialise cipher */
	rc = cipher_setkey ( cipher, ctx, key, key_len );
	assert ( rc == 0 );
	cipher_setiv ( cipher, ctx, iv );

	/* Perform encryption */
	cipher_decrypt ( cipher, ctx, ciphertext, plaintext, len );

	/* Verify result */
	return ( memcmp ( plaintext, expected_plaintext, len ) == 0 );
}
