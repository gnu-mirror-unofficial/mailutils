/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2004 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <mailutils/mailutils.h>
#include <mailutils/sql.h>

#include <mysql/mysql.h>
#include <mysql/errmsg.h>

struct mu_mysql_data
{
  MYSQL *mysql;
  MYSQL_RES  *result;
};
  

static int 
do_mysql_query (mu_sql_connection_t conn, char *query)
{
  int rc;
  int i;
  MYSQL *mysql;

  for (i = 0; i < 10; i++)
    {
      mysql = ((struct mu_mysql_data*)conn->data)->mysql;
      rc = mysql_query (mysql, query);
      if (rc && mysql_errno (mysql) == CR_SERVER_GONE_ERROR)
	{
	  /* Reconnect? */
	  mu_sql_disconnect (conn);
	  mu_sql_connect (conn);
	  continue;
	}
      break;
    }
  return rc;
}

/* ************************************************************************* */
/* Interface routines */

static int
init (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = calloc (1, sizeof (*mp));
  if (!mp)
    return ENOMEM;
  conn->data = mp;
  return 0;
}

static int
destroy (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  free (mp->mysql);
  free (mp);
  conn->data = NULL;
  return 0;
}
  

static int
connect (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  char *host, *socket_name;
  
  mp->mysql = malloc (sizeof(MYSQL));
  if (!mp->mysql)
    return ENOMEM;
  
  mysql_init (mp->mysql);

  if (conn->server[0] == '/')
    {
      host = "localhost";
      socket_name = conn->server;
    }
  else
    host = conn->server;
  
  if (!mysql_real_connect(mp->mysql, 
			  host,
			  conn->login,
			  conn->password,
			  conn->dbname,
			  conn->port,
			  socket_name,
			  0))
    return MU_ERR_SQL;
  
  return 0;
}

static int 
disconnect (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  
  mysql_close (mp->mysql);
  free (mp->mysql);
  mp->mysql = NULL;  
  return 0;
}

static int
query (mu_sql_connection_t conn, char *query)
{
  if (do_mysql_query (conn, query)) 
    return MU_ERR_SQL;
  return 0;
}


static int
store_result (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  if (!(mp->result = mysql_store_result (mp->mysql)))
    {
      if (mysql_errno (mp->mysql))
	return MU_ERR_SQL;
      return MU_ERR_NO_RESULT;
    }
  return 0;
}

static int
release_result (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  mysql_free_result (mp->result);
  return 0;
}

static int
num_columns (mu_sql_connection_t conn, size_t *np)
{
  struct mu_mysql_data *mp = conn->data;
  *np = mysql_num_fields (mp->result);
  return 0;
}

static int
num_tuples (mu_sql_connection_t conn, size_t *np)
{
  struct mu_mysql_data *mp = conn->data;
  *np = mysql_num_rows (mp->result);
  return 0;
}

static int
get_column (mu_sql_connection_t conn, size_t nrow, size_t ncol, char **pdata)
{
  struct mu_mysql_data *mp = conn->data;
  MYSQL_ROW row;

  if (nrow >= mysql_num_rows (mp->result)
      || ncol >= mysql_num_fields (mp->result))
    return MU_ERR_BAD_COLUMN;
  
  mysql_data_seek (mp->result, nrow);
  row = mysql_fetch_row (mp->result);
  if (!row)
    return MU_ERR_BAD_COLUMN;
  *pdata = row[ncol];
  return 0;
}

static char *
errstr (mu_sql_connection_t conn)
{
  struct mu_mysql_data *mp = conn->data;
  return mysql_error (mp->mysql);
}

MU_DECL_SQL_DISPATCH_T(mysql) = {
  "mysql",
  3306,
  init,
  destroy,
  connect,
  disconnect,
  query,
  store_result,
  release_result,
  num_tuples,
  num_columns,
  get_column,
  errstr,
};
