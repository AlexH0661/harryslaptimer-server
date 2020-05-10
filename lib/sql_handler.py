import threading
import logging

#+--------------------------------+
#| Required external libraries    |
#+--------------------------------+
import mysql.connector

#+--------------------------------------------------------------+
#| MySQL Threaded Connection                                    |
#+--------------------------------------------------------------+
class mysql_connection(threading.Thread):
    def __init__(self,  _mysqlhost, _mysqlport, _username, _password):
        threading.Thread.__init__(self)
        self._mysqlhost = _mysqlhost
        self._mysqlport = _mysqlport
        self._username = _username
        self._password = _password
    def run(self):
        logging.info('Attempting to connect {0}@{1}:{2}'.format(self._username, self._mysqlhost, self._mysqlport))
        connection = mysql.connector.connect(host=self._mysqlhost,
                                port=self._mysqlport,
                                user=self._username,
                                password=self._password,
                                database='HLT_Server',
                                auth_plugin='mysql_native_password')
        try:
            if connection.is_connected():
                db_Info = connection.get_server_info()
                logging.info("Connected to MySQL Server version {0}".format(db_Info))
                cursor = connection.cursor()
                cursor.execute("select database();")
                record = cursor.fetchone()
                logging.info("You're connected to database: {0}".format(record))
        except BaseException as e:
            logging.error(e)
        finally:
            if (connection.is_connected()):
                cursor.close()
                connection.close()
                logging.info("MySQL connection is closed")
