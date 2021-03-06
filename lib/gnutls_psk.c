/*
 * Copyright (C) 2005, 2007, 2008, 2009, 2010 Free Software Foundation,
 * Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA
 *
 */

/* Functions for manipulating the PSK credentials. */

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <auth_psk.h>
#include <gnutls_state.h>

#ifdef ENABLE_PSK

#include <auth_psk_passwd.h>
#include <gnutls_num.h>
#include <gnutls_helper.h>
#include <gnutls_datum.h>
#include "debug.h"

/**
 * gnutls_psk_free_client_credentials:
 * @sc: is a #gnutls_psk_client_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to free (deallocate) it.
 **/
void
gnutls_psk_free_client_credentials (gnutls_psk_client_credentials_t sc)
{
  _gnutls_free_datum (&sc->username);
  _gnutls_free_datum (&sc->key);
  gnutls_free (sc);
}

/**
 * gnutls_psk_allocate_client_credentials:
 * @sc: is a pointer to a #gnutls_psk_server_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to allocate it.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_psk_allocate_client_credentials (gnutls_psk_client_credentials_t * sc)
{
  *sc = gnutls_calloc (1, sizeof (psk_client_credentials_st));

  if (*sc == NULL)
    return GNUTLS_E_MEMORY_ERROR;

  return 0;
}

/**
 * gnutls_psk_set_client_credentials:
 * @res: is a #gnutls_psk_client_credentials_t structure.
 * @username: is the user's zero-terminated userid
 * @key: is the user's key
 * @format: indicate the format of the key, either
 *   %GNUTLS_PSK_KEY_RAW or %GNUTLS_PSK_KEY_HEX.
 *
 * This function sets the username and password, in a
 * gnutls_psk_client_credentials_t structure.  Those will be used in
 * PSK authentication.  @username should be an ASCII string or UTF-8
 * strings prepared using the "SASLprep" profile of "stringprep".  The
 * key can be either in raw byte format or in Hex format (without the
 * 0x prefix).
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_psk_set_client_credentials (gnutls_psk_client_credentials_t res,
				   const char *username,
				   const gnutls_datum_t * key,
				   gnutls_psk_key_flags flags)
{
  int ret;

  if (username == NULL || key == NULL || key->data == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_INVALID_REQUEST;
    }

  ret = _gnutls_set_datum (&res->username, username, strlen (username));
  if (ret < 0)
    return ret;

  if (flags == GNUTLS_PSK_KEY_RAW)
    {
      if (_gnutls_set_datum (&res->key, key->data, key->size) < 0)
	{
	  gnutls_assert ();
	  ret = GNUTLS_E_MEMORY_ERROR;
	  goto error;
	}
    }
  else
    {				/* HEX key */
      size_t size;
      size = res->key.size = key->size / 2;
      res->key.data = gnutls_malloc (size);
      if (res->key.data == NULL)
	{
	  gnutls_assert ();
	  ret = GNUTLS_E_MEMORY_ERROR;
	  goto error;
	}

      ret = gnutls_hex_decode (key, (char *) res->key.data, &size);
      res->key.size = (unsigned int) size;
      if (ret < 0)
	{
	  gnutls_assert ();
	  goto error;
	}

    }

  return 0;

error:
  _gnutls_free_datum (&res->username);

  return ret;
}

/**
 * gnutls_psk_free_server_credentials:
 * @sc: is a #gnutls_psk_server_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to free (deallocate) it.
 **/
void
gnutls_psk_free_server_credentials (gnutls_psk_server_credentials_t sc)
{
  gnutls_free (sc->password_file);
  gnutls_free (sc);
}

/**
 * gnutls_psk_allocate_server_credentials:
 * @sc: is a pointer to a #gnutls_psk_server_credentials_t structure.
 *
 * This structure is complex enough to manipulate directly thus this
 * helper function is provided in order to allocate it.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_psk_allocate_server_credentials (gnutls_psk_server_credentials_t * sc)
{
  *sc = gnutls_calloc (1, sizeof (psk_server_cred_st));

  if (*sc == NULL)
    return GNUTLS_E_MEMORY_ERROR;

  return 0;
}


/**
 * gnutls_psk_set_server_credentials_file:
 * @res: is a #gnutls_psk_server_credentials_t structure.
 * @password_file: is the PSK password file (passwd.psk)
 *
 * This function sets the password file, in a
 * %gnutls_psk_server_credentials_t structure.  This password file
 * holds usernames and keys and will be used for PSK authentication.
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 **/
int
gnutls_psk_set_server_credentials_file (gnutls_psk_server_credentials_t
					res, const char *password_file)
{

  if (password_file == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_INVALID_REQUEST;
    }

  /* Check if the files can be opened */
  if (_gnutls_file_exists (password_file) != 0)
    {
      gnutls_assert ();
      return GNUTLS_E_FILE_ERROR;
    }

  res->password_file = gnutls_strdup (password_file);
  if (res->password_file == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MEMORY_ERROR;
    }

  return 0;
}

/**
 * gnutls_psk_set_server_credentials_hint:
 * @res: is a #gnutls_psk_server_credentials_t structure.
 * @hint: is the PSK identity hint string
 *
 * This function sets the identity hint, in a
 * %gnutls_psk_server_credentials_t structure.  This hint is sent to
 * the client to help it chose a good PSK credential (i.e., username
 * and password).
 *
 * Returns: %GNUTLS_E_SUCCESS on success, or an error code.
 *
 * Since: 2.4.0
 **/
int
gnutls_psk_set_server_credentials_hint (gnutls_psk_server_credentials_t res,
					const char *hint)
{
  res->hint = gnutls_strdup (hint);
  if (res->hint == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MEMORY_ERROR;
    }

  return 0;
}

/**
 * gnutls_psk_set_server_credentials_function:
 * @cred: is a #gnutls_psk_server_credentials_t structure.
 * @func: is the callback function
 *
 * This function can be used to set a callback to retrieve the user's PSK credentials.
 * The callback's function form is:
 * int (*callback)(gnutls_session_t, const char* username,
 *  gnutls_datum_t* key);
 *
 * @username contains the actual username.
 * The @key must be filled in using the gnutls_malloc().
 *
 * In case the callback returned a negative number then gnutls will
 * assume that the username does not exist.
 *
 * The callback function will only be called once per handshake.  The
 * callback function should return 0 on success, while -1 indicates
 * an error.
 **/
void
gnutls_psk_set_server_credentials_function (gnutls_psk_server_credentials_t
					    cred,
					    gnutls_psk_server_credentials_function
					    * func)
{
  cred->pwd_callback = func;
}

/**
 * gnutls_psk_set_client_credentials_function:
 * @cred: is a #gnutls_psk_server_credentials_t structure.
 * @func: is the callback function
 *
 * This function can be used to set a callback to retrieve the username and
 * password for client PSK authentication.
 * The callback's function form is:
 * int (*callback)(gnutls_session_t, char** username,
 *  gnutls_datum_t* key);
 *
 * The @username and @key->data must be allocated using gnutls_malloc().
 * @username should be ASCII strings or UTF-8 strings prepared using
 * the "SASLprep" profile of "stringprep".
 *
 * The callback function will be called once per handshake.
 *
 * The callback function should return 0 on success.
 * -1 indicates an error.
 **/
void
gnutls_psk_set_client_credentials_function (gnutls_psk_client_credentials_t
					    cred,
					    gnutls_psk_client_credentials_function
					    * func)
{
  cred->get_function = func;
}


/**
 * gnutls_psk_server_get_username:
 * @session: is a gnutls session
 *
 * This should only be called in case of PSK authentication and in
 * case of a server.
 *
 * Returns: the username of the peer, or %NULL in case of an error.
 **/
const char *
gnutls_psk_server_get_username (gnutls_session_t session)
{
  psk_auth_info_t info;

  CHECK_AUTH (GNUTLS_CRD_PSK, NULL);

  info = _gnutls_get_auth_info (session);
  if (info == NULL)
    return NULL;

  if (info->username[0] != 0)
    return info->username;

  return NULL;
}

/**
 * gnutls_psk_client_get_hint:
 * @session: is a gnutls session
 *
 * The PSK identity hint may give the client help in deciding which
 * username to use.  This should only be called in case of PSK
 * authentication and in case of a client.
 *
 * Returns: the identity hint of the peer, or %NULL in case of an error.
 *
 * Since: 2.4.0
 **/
const char *
gnutls_psk_client_get_hint (gnutls_session_t session)
{
  psk_auth_info_t info;

  CHECK_AUTH (GNUTLS_CRD_PSK, NULL);

  info = _gnutls_get_auth_info (session);
  if (info == NULL)
    return NULL;

  if (info->hint[0] != 0)
    return info->hint;

  return NULL;
}

/**
 * gnutls_hex_decode:
 * @hex_data: contain the encoded data
 * @result: the place where decoded data will be copied
 * @result_size: holds the size of the result
 *
 * This function will decode the given encoded data, using the hex
 * encoding used by PSK password files.
 *
 * Note that hex_data should be null terminated.
 *
 * Returns: %GNUTLS_E_SHORT_MEMORY_BUFFER if the buffer given is not
 *   long enough, or 0 on success.
 **/
int
gnutls_hex_decode (const gnutls_datum_t * hex_data, char *result,
		   size_t * result_size)
{
  int ret;

  ret =
    _gnutls_hex2bin (hex_data->data, hex_data->size, (opaque *) result,
		     result_size);
  if (ret < 0)
    return ret;

  return 0;
}

/**
 * gnutls_hex_encode:
 * @data: contain the raw data
 * @result: the place where hex data will be copied
 * @result_size: holds the size of the result
 *
 * This function will convert the given data to printable data, using
 * the hex encoding, as used in the PSK password files.
 *
 * Returns: %GNUTLS_E_SHORT_MEMORY_BUFFER if the buffer given is not
 * long enough, or 0 on success.
 **/
int
gnutls_hex_encode (const gnutls_datum_t * data, char *result,
		   size_t * result_size)
{
  size_t res = data->size + data->size + 1;

  if (*result_size < res)
    {
      gnutls_assert ();
      return GNUTLS_E_SHORT_MEMORY_BUFFER;
    }

  _gnutls_bin2hex (data->data, data->size, result, *result_size);
  *result_size = res;

  return 0;
}

/**
 * gnutls_psk_set_server_dh_params:
 * @res: is a gnutls_psk_server_credentials_t structure
 * @dh_params: is a structure that holds Diffie-Hellman parameters.
 *
 * This function will set the Diffie-Hellman parameters for an
 * anonymous server to use. These parameters will be used in
 * Diffie-Hellman exchange with PSK cipher suites.
 **/
void
gnutls_psk_set_server_dh_params (gnutls_psk_server_credentials_t res,
				 gnutls_dh_params_t dh_params)
{
  res->dh_params = dh_params;
}

/**
 * gnutls_psk_set_server_params_function:
 * @res: is a #gnutls_certificate_credentials_t structure
 * @func: is the function to be called
 *
 * This function will set a callback in order for the server to get
 * the Diffie-Hellman parameters for PSK authentication.  The callback
 * should return zero on success.
 **/
void
gnutls_psk_set_server_params_function (gnutls_psk_server_credentials_t res,
				       gnutls_params_function * func)
{
  res->params_func = func;
}

#endif /* ENABLE_PSK */
