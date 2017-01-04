/*
 * p11test_case_readonly.c: Sign & Verify tests
 *
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * Author: Jakub Jelen <jjelen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "p11test_case_readonly.h"

unsigned char *rsa_x_509_pad_message(unsigned char *message,
	unsigned long *message_length, test_cert_t *o)
{
	int pad_message_length = (o->bits+7)/8;
	unsigned char *pad_message = malloc(pad_message_length);
	RSA_padding_add_PKCS1_type_1(pad_message, pad_message_length,
	    message, *message_length);
	free(message);
	*message_length = pad_message_length;
	return pad_message;
}

int encrypt_message_openssl(test_cert_t *o, token_info_t *info, CK_BYTE *message,
    CK_ULONG message_length, test_mech_t *mech, unsigned char **enc_message)
{
	int rv, padding;

	*enc_message = malloc(RSA_size(o->key.rsa));
	if (*enc_message == NULL) {
		debug_print("malloc returned null");
		return -1;
	}

	/* Prepare padding for RSA_X_509 */
	padding = ((mech->mech == CKM_RSA_X_509) ? RSA_NO_PADDING : RSA_PKCS1_PADDING);
	rv = RSA_public_encrypt(message_length, message,
		*enc_message, o->key.rsa, padding);
	if (rv < 0) {
		free(*enc_message);
		debug_print("RSA_public_encrypt: rv = 0x%.8X\n", rv);
		return -1;
	}
	return rv;
}

int encrypt_message(test_cert_t *o, token_info_t *info, CK_BYTE *message,
    CK_ULONG message_length, test_mech_t *mech, unsigned char **enc_message)
{
	CK_RV rv;
	CK_FUNCTION_LIST_PTR fp = info->function_pointer;
	CK_MECHANISM enc_mechanism = { mech->mech, NULL_PTR, 0 };
	CK_ULONG enc_message_length;

	rv = fp->C_EncryptInit(info->session_handle, &enc_mechanism,
		o->public_handle);
	if (rv != CKR_OK) {
		debug_print("   C_EncryptInit: rv = 0x%.8lX", rv);
		goto openssl_encrypt;
	}

	/* get the expected length */
	rv = fp->C_Encrypt(info->session_handle, message, message_length,
	    NULL, &enc_message_length);
	if (rv != CKR_OK) {
		debug_print("   C_Encrypt: rv = 0x%.8lX", rv);
		goto openssl_encrypt;
	}
	*enc_message = malloc(enc_message_length);
	if (*enc_message == NULL) {
		debug_print("malloc returned null");
		return -1;
	}

	/* Do the actual encryption with allocated buffer */
	rv = fp->C_Encrypt(info->session_handle, message, message_length,
		*enc_message, &enc_message_length);
	if (rv == CKR_OK) {
		mech->flags |= FLAGS_VERIFY_SIGN;
		return enc_message_length;
	}
	debug_print("   C_Encrypt: rv = 0x%.8lX", rv);

openssl_encrypt:
	debug_print(" [ KEY %s ] Falling back to openssl encryption", o->id_str);
	return encrypt_message_openssl(o, info, message, message_length, mech,
	    enc_message);
}

int decrypt_message(test_cert_t *o, token_info_t *info, CK_BYTE *enc_message,
    CK_ULONG enc_message_length, test_mech_t *mech, unsigned char **dec_message)
{
	CK_RV rv;
	CK_FUNCTION_LIST_PTR fp = info->function_pointer;
	CK_MECHANISM dec_mechanism = { mech->mech, NULL_PTR, 0 };
	CK_ULONG dec_message_length = BUFFER_SIZE;

	rv = fp->C_DecryptInit(info->session_handle, &dec_mechanism,
		o->private_handle);
	if (rv == CKR_KEY_TYPE_INCONSISTENT) {
		debug_print(" [SKIP %s ] Not allowed to decrypt with this key?", o->id_str);
		return 0;
	} else if (rv != CKR_OK) {
		debug_print("C_DecryptInit: rv = 0x%.8lX\n", rv);
		return -1;
	}

	*dec_message = malloc(dec_message_length);

	always_authenticate(o, info);

	rv = fp->C_Decrypt(info->session_handle, enc_message,
		enc_message_length, *dec_message, &dec_message_length);
	if (rv != CKR_OK) {
		free(*dec_message);
		debug_print("  C_Decrypt: rv = 0x%.8lX\n", rv);
		return -1;
	}
	return (int) dec_message_length;
}

/* Perform encryption and decryption of a message using private key referenced
 * in the  o  object with mechanism defined by  mech.
 *
 * NONE of the reasonable mechanisms support multipart encryption/decryption
 *
 * Returns
 *  * 1 for successful Encrypt&Decrypt sequnce
 *  * 0 for skipped test (unsupporedted mechanism, key, ...)
 *  * -1 otherwise.
 *  Serious errors terminate the execution.
 */
int encrypt_decrypt_test(test_cert_t *o, token_info_t *info, test_mech_t *mech,
	CK_ULONG message_length, int multipart)
{
	CK_BYTE *message = (CK_BYTE *) strdup(SHORT_MESSAGE_TO_SIGN);
	CK_BYTE *dec_message = NULL;
	int dec_message_length = 0;
	unsigned char *enc_message;
	int enc_message_length, rv;

	if (o->type != EVP_PK_RSA) {
		debug_print(" [ KEY %s ] Skip non-RSA key for encryption", o->id_str);
		return 0;
	}
	/* XXX other supported encryption mechanisms */
	if (mech->mech != CKM_RSA_X_509 && mech->mech != CKM_RSA_PKCS) {
		debug_print(" [ KEY %s ] Skip encryption for non-supported mechanism", o->id_str);
		return 0;
	}

	if (mech->mech == CKM_RSA_X_509)
		message = rsa_x_509_pad_message(message, &message_length, o);

	debug_print(" [ KEY %s ] Encrypt message using CKM_%s",
		o->id_str, get_mechanism_name(mech->mech));
	enc_message_length = encrypt_message(o, info, message, message_length,
	    mech, &enc_message);
	if (enc_message_length <= 0) {
		free(message);
		return -1;
	}

	debug_print(" [ KEY %s ] Decrypt message", o->id_str);
	dec_message_length = decrypt_message(o, info, enc_message,
	    enc_message_length, mech, &dec_message);
	free(enc_message);
	if (dec_message_length <= 0) {
		free(message);
		return -1;
	}

	if (memcmp(dec_message, message, dec_message_length) == 0
			&& (unsigned int) dec_message_length == message_length) {
		debug_print(" [  OK %s ] Text decrypted successfully.", o->id_str);
		mech->flags |= FLAGS_VERIFY_DECRYPT;
		rv = 1;
	} else {
		dec_message[dec_message_length] = '\0';
		debug_print(" [ ERROR %s ] Text decryption failed. Recovered text: %s",
			o->id_str, dec_message);
		rv = 0;
	}
	free(dec_message);
	if (mech->mech == CKM_RSA_X_509)
		free(message);
	return rv;
}

int sign_message(test_cert_t *o, token_info_t *info, CK_BYTE *message,
    CK_ULONG message_length, test_mech_t *mech, unsigned char **sign,
    int multipart)
{
	CK_RV rv;
	CK_FUNCTION_LIST_PTR fp = info->function_pointer;
	CK_MECHANISM sign_mechanism = { mech->mech, NULL_PTR, 0 };
	CK_ULONG sign_length = 0;
	char *name;

	rv = fp->C_SignInit(info->session_handle, &sign_mechanism,
		o->private_handle);
	if (rv == CKR_KEY_TYPE_INCONSISTENT) {
		debug_print(" [SKIP %s ] Not allowed to sign with this key?", o->id_str);
		return 0;
	} else if (rv == CKR_MECHANISM_INVALID) {
		debug_print(" [SKIP %s ] Bad mechanism. Not supported?", o->id_str);
		return 0;
	} else if (rv != CKR_OK) {
		debug_print("  C_SignInit: rv = 0x%.8lX\n", rv);
		return -1;
	}

	always_authenticate(o, info);

	if (multipart) {
		int part = message_length / 3;
		rv = fp->C_SignUpdate(info->session_handle, message, part);
		if (rv != CKR_OK) {
			fprintf(stderr, "  C_SignUpdate: rv = 0x%.8lX\n", rv);
			return -1;
		}
		rv = fp->C_SignUpdate(info->session_handle, message + part, message_length - part);
		if (rv != CKR_OK) {
			fprintf(stderr, "  C_SignUpdate: rv = 0x%.8lX\n", rv);
			return -1;
		}
		/* Call C_SignFinal with NULL argument to find out the real size of signature */
		rv = fp->C_SignFinal(info->session_handle, *sign, &sign_length);
		if (rv != CKR_OK) {
			fprintf(stderr, "  C_SignFinal: rv = 0x%.8lX\n", rv);
			return -1;
		}

		*sign = malloc(sign_length);
		if (*sign == NULL) {
			fprintf(stderr, "%s: malloc failed", __func__);
			return -1;
		}

		/* Call C_SignFinal with allocated buffer to the the actual signature */
		rv = fp->C_SignFinal(info->session_handle, *sign, &sign_length);
		name = "C_SignFinal";
	} else {
		/* Call C_Sign with NULL argument to find out the real size of signature */
		rv = fp->C_Sign(info->session_handle,
			message, message_length, *sign, &sign_length);
		if (rv != CKR_OK) {
			fprintf(stderr, "  C_Sign: rv = 0x%.8lX\n", rv);
			return -1;
		}

		*sign = malloc(sign_length);
		if (*sign == NULL) {
			fprintf(stderr, "%s: malloc failed", __func__);
			return -1;
		}

		/* Call C_Sign with allocated buffer to the the actual signature */
		rv = fp->C_Sign(info->session_handle,
			message, message_length, *sign, &sign_length);
		name = "C_Sign";
	}
	if (rv != CKR_OK) {
		free(*sign);
		fprintf(stderr, "  %s: rv = 0x%.8lX\n", name, rv);
		return -1;
	}
	return sign_length;
}

int verify_message_openssl(test_cert_t *o, token_info_t *info, CK_BYTE *message,
    CK_ULONG message_length, test_mech_t *mech, unsigned char *sign,
    CK_ULONG sign_length)
{
	CK_RV rv;
	CK_BYTE *cmp_message = NULL;
	int cmp_message_length;

	if (o->type == EVP_PK_RSA) {
		int type;

		/* raw RSA mechanism */
		if (mech->mech == CKM_RSA_PKCS || mech->mech == CKM_RSA_X_509) {
			CK_BYTE dec_message[BUFFER_SIZE];
			int padding = ((mech->mech == CKM_RSA_X_509)
				? RSA_NO_PADDING : RSA_PKCS1_PADDING);
			int dec_message_length = RSA_public_decrypt(sign_length, sign,
				dec_message, o->key.rsa, padding);
			if (dec_message_length < 0) {
				fprintf(stderr, "RSA_public_decrypt: rv = %d: %s\n", dec_message_length,
					ERR_error_string(ERR_peek_last_error(), NULL));
				return -1;
			}
			if (memcmp(dec_message, message, dec_message_length) == 0
					&& dec_message_length == (int) message_length) {
				debug_print(" [  OK %s ] Signature is valid.", o->id_str);
				mech->flags |= FLAGS_VERIFY_SIGN;
				return 1;
			} else {
				fprintf(stderr, " [ ERROR %s ] Signature is not valid. Error: %s",
					o->id_str, ERR_error_string(ERR_peek_last_error(), NULL));
				return 0;
			}
		}

		/* Digest mechanisms */
		switch (mech->mech) {
			case CKM_SHA1_RSA_PKCS:
				cmp_message = SHA1(message, message_length, NULL);
				cmp_message_length = SHA_DIGEST_LENGTH;
				type = NID_sha1;
				break;
			case CKM_SHA224_RSA_PKCS:
				cmp_message = SHA224(message, message_length, NULL);
				cmp_message_length = SHA224_DIGEST_LENGTH;
				type = NID_sha224;
				break;
			case CKM_SHA256_RSA_PKCS:
				cmp_message = SHA256(message, message_length, NULL);
				cmp_message_length = SHA256_DIGEST_LENGTH;
				type = NID_sha256;
				break;
			case CKM_SHA384_RSA_PKCS:
				cmp_message = SHA384(message, message_length, NULL);
				cmp_message_length = SHA384_DIGEST_LENGTH;
				type = NID_sha384;
				break;
			case CKM_SHA512_RSA_PKCS:
				cmp_message = SHA512(message, message_length, NULL);
				cmp_message_length = SHA512_DIGEST_LENGTH;
				type = NID_sha512;
				break;
			case CKM_MD5_RSA_PKCS:
				cmp_message = MD5(message, message_length, NULL);
				cmp_message_length = MD5_DIGEST_LENGTH;
				type = NID_md5;
				break;
			case CKM_RIPEMD160_RSA_PKCS:
				cmp_message = RIPEMD160(message, message_length, NULL);
				cmp_message_length = RIPEMD160_DIGEST_LENGTH;
				type = NID_ripemd160;
				break;
			default:
				debug_print(" [SKIP %s ] Skip verify of unknown mechanism", o->id_str);
				return 0;
		}
		rv = RSA_verify(type, cmp_message, cmp_message_length,
			sign, sign_length, o->key.rsa);
		if (rv == 1) {
			debug_print(" [  OK %s ] Signature is valid.", o->id_str);
			mech->flags |= FLAGS_VERIFY_SIGN;
		 } else {
			fprintf(stderr, " [ ERROR %s ] Signature is not valid. Error: %s",
				o->id_str, ERR_error_string(ERR_peek_last_error(), NULL));
			return -1;
		}
	} else if (o->type == EVP_PK_EC) {
		unsigned int nlen;
		ECDSA_SIG *sig = ECDSA_SIG_new();
		if (sig == NULL) {
			fprintf(stderr, "ECDSA_SIG_new: failed");
			return -1;
		}
		nlen = sign_length/2;
		BN_bin2bn(&sign[0], nlen, sig->r);
		BN_bin2bn(&sign[nlen], nlen, sig->s);
		if (mech->mech == CKM_ECDSA_SHA1) {
			cmp_message = SHA1(message, message_length, NULL);
			cmp_message_length = SHA_DIGEST_LENGTH;
		} else {
			cmp_message = message;
			cmp_message_length = message_length;
		}
		rv = ECDSA_do_verify(cmp_message, cmp_message_length, sig, o->key.ec);
		if (rv == 1) {
			ECDSA_SIG_free(sig);
			debug_print(" [  OK %s ] EC Signature of length %lu is valid.",
				o->id_str, message_length);
			mech->flags |= FLAGS_VERIFY_SIGN;
			return 1;
		} else {
			ECDSA_SIG_free(sig);
			fprintf(stderr, " [FAIL %s ] ECDSA_do_verify: rv = %lu: %s\n", o->id_str,
				rv, ERR_error_string(ERR_peek_last_error(), NULL));
			return -1;
		}
	} else {
		fprintf(stderr, " [ KEY %s ] Unknown type. Not verifying", o->id_str);
	}
	return 0;
}

int verify_message(test_cert_t *o, token_info_t *info, CK_BYTE *message,
    CK_ULONG message_length, test_mech_t *mech, unsigned char *sign,
    CK_ULONG sign_length, int multipart)
{
	CK_RV rv;
	CK_FUNCTION_LIST_PTR fp = info->function_pointer;
	CK_MECHANISM sign_mechanism = { mech->mech, NULL_PTR, 0 };
	char *name;

	/* try C_Verify() if it is supported */
	rv = fp->C_VerifyInit(info->session_handle, &sign_mechanism,
		o->public_handle);
	if (rv != CKR_OK) {
		debug_print("   C_VerifyInit: rv = 0x%.8lX", rv);
		goto openssl_verify;
	}
	if (multipart) {
		int part = message_length / 3;
		/* First part */
		rv = fp->C_VerifyUpdate(info->session_handle, message, part);
		if (rv != CKR_OK) {
			debug_print("   C_VerifyUpdate: rv = 0x%.8lX", rv);
			goto openssl_verify;
		}
		/* Second part */
		rv = fp->C_VerifyUpdate(info->session_handle, message + part,
		    message_length - part);
		if (rv != CKR_OK) {
			debug_print("   C_VerifyUpdate: rv = 0x%.8lX", rv);
			goto openssl_verify;
		}
		/* Final */
		rv = fp->C_VerifyFinal(info->session_handle,
			sign, sign_length);
		name = "C_VerifyFinal";
	} else {
		rv = fp->C_Verify(info->session_handle,
			message, message_length, sign, sign_length);
		name = "C_Verify";
	}
	if (rv == CKR_OK) {
		mech->flags |= FLAGS_VERIFY_SIGN;
		return 1;
	}
	debug_print("   %s: rv = 0x%.8lX", name, rv);

openssl_verify:
	debug_print(" [ KEY %s ] Falling back to openssl verification", o->id_str);
	return verify_message_openssl(o, info, message, message_length, mech,
		sign, sign_length);
}

/* Perform signature and verication of a message using private key referenced
 * in the  o  object with mechanism defined by  mech. Message length can be
 * specified using argument  message_length.
 *
 * Returns
 *  * 1 for successful Encrypt&Decrypt sequnce
 *  * 0 for skipped test (unsupporedted mechanism, key, ...)
 *  * -1 otherwise.
 *  Serious errors terminate the execution.
 */
int sign_verify_test(test_cert_t *o, token_info_t *info, test_mech_t *mech,
    CK_ULONG message_length, int multipart)
{
	CK_BYTE *message = (CK_BYTE *) strdup(SHORT_MESSAGE_TO_SIGN);
	CK_BYTE *sign = NULL;
	CK_ULONG sign_length = 0;
	int rv = 0;

	if (message_length > strlen((char *)message))
		fail_msg("Truncate is longer than the actual message");

	if (o->private_handle == CK_INVALID_HANDLE) {
		debug_print(" [SKIP %s ] Missing private key", o->id_str);
		return 0;
	}

	if (o->type != EVP_PK_EC && o->type != EVP_PK_RSA) {
		debug_print(" [SKIP %s ] Skip non-RSA and non-EC key", o->id_str);
		return 0;
	}

	if (mech->mech == CKM_RSA_X_509)
		message = rsa_x_509_pad_message(message, &message_length, o);

	debug_print(" [ KEY %s ] Signing message of length %lu using CKM_%s",
		o->id_str, message_length, get_mechanism_name(mech->mech));
	rv = sign_message(o, info, message, message_length, mech, &sign, multipart);
	if (rv <= 0) {
		free(message);
		return rv;
	}
	sign_length = (unsigned long) rv;

	debug_print(" [ KEY %s ] Verify message signature", o->id_str);
	rv = verify_message(o, info, message, message_length, mech,
		sign, sign_length, multipart);
	free(sign);
	if (mech->mech == CKM_RSA_X_509)
		free(message);
	return rv;
}

void readonly_tests(void **state) {

	token_info_t *info = (token_info_t *) *state;
	unsigned int i;
	int used, j;
	test_certs_t objects;

	objects.count = 0;
	objects.data = NULL;

	search_for_all_objects(&objects, info);

	P11TEST_START(info);
	debug_print("\nCheck functionality of Sign&Verify and/or Encrypt&Decrypt");
	for (i = 0; i < objects.count; i++) {
		used = 0;
		/* do the Sign&Verify and/or Encrypt&Decrypt */
		/* XXX some keys do not have appropriate flags, but we can use them
		 * or vice versa */
		//if (objects.data[i].sign && objects.data[i].verify)
			for (j = 0; j < objects.data[i].num_mechs; j++)
				used |= sign_verify_test(&(objects.data[i]), info,
					&(objects.data[i].mechs[j]), 32, 0);

		//if (objects.data[i].encrypt && objects.data[i].decrypt)
			for (j = 0; j < objects.data[i].num_mechs; j++)
				used |= encrypt_decrypt_test(&(objects.data[i]), info,
					&(objects.data[i].mechs[j]), 32, 0);

		if (!used) {
			debug_print(" [ WARN %s ] Private key with unknown purpose T:%02lX",
			objects.data[i].id_str, objects.data[i].key_type);
		}
	}

	if (objects.count == 0) {
		printf(" [WARN] No objects to display\n");
		return;
	}

	/* print summary */
	printf("[KEY ID] [LABEL]\n");
	printf("[ TYPE ] [ SIZE ] [PUBLIC] [SIGN&VERIFY] [ENC&DECRYPT] [WRAP&UNWR] [ DERIVE ]\n");
	P11TEST_DATA_ROW(info, 4,
		's', "KEY ID",
		's', "MECHANISM",
		's', "SIGN&VERIFY WORKS",
		's', "ENCRYPT&DECRYPT WORKS");
	for (i = 0; i < objects.count; i++) {
		printf("\n[%-6s] [%s]\n",
			objects.data[i].id_str,
			objects.data[i].label);
		printf("[ %s ] [%6lu] [ %s ] [%s%s] [%s%s] [%s %s] [%s%s]\n",
			objects.data[i].key_type == CKK_RSA ? "RSA " :
				objects.data[i].key_type == CKK_EC ? " EC " : " ?? ",
			objects.data[i].bits,
			objects.data[i].verify_public == 1 ? " ./ " : "    ",
			objects.data[i].sign ? "[./] " : "[  ] ",
			objects.data[i].verify ? " [./] " : " [  ] ",
			objects.data[i].encrypt ? "[./] " : "[  ] ",
			objects.data[i].decrypt ? " [./] " : " [  ] ",
			objects.data[i].wrap ? "[./]" : "[  ]",
			objects.data[i].unwrap ? "[./]" : "[  ]",
			objects.data[i].derive_pub ? "[./]" : "[  ]",
			objects.data[i].derive_priv ? "[./]" : "[  ]");
		for (j = 0; j < objects.data[i].num_mechs; j++) {
			printf("  [ %-20s ] [   %s    ] [   %s    ] [         ] [        ]\n",
				get_mechanism_name(objects.data[i].mechs[j].mech),
				objects.data[i].mechs[j].flags & FLAGS_VERIFY_SIGN ? "[./]" : "    ",
				objects.data[i].mechs[j].flags & FLAGS_VERIFY_DECRYPT ? "[./]" : "    ");
			if ((objects.data[i].mechs[j].flags & FLAGS_VERIFY_SIGN) == 0 &&
				(objects.data[i].mechs[j].flags & FLAGS_VERIFY_DECRYPT) == 0)
				continue; /* skip emty rows for export */
			P11TEST_DATA_ROW(info, 4,
				's', objects.data[i].id_str,
				's', get_mechanism_name(objects.data[i].mechs[j].mech),
				's', objects.data[i].mechs[j].flags & FLAGS_VERIFY_SIGN ? "YES" : "",
				's', objects.data[i].mechs[j].flags & FLAGS_VERIFY_DECRYPT ? "YES" : "");
		}
	}
	printf(" Public == Cert -----^       ^  ^  ^       ^  ^  ^       ^----^- Attributes\n");
	printf(" Sign Attribute -------------'  |  |       |  |  '---- Decrypt Attribute\n");
	printf(" Sign&Verify functionality -----'  |       |  '------- Enc&Dec functionality\n");
	printf(" Verify Attribute -----------------'       '---------- Encrypt Attribute\n");

	clean_all_objects(&objects);
	P11TEST_PASS(info);
}