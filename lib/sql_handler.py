import threading
import logging

#+--------------------------------+
#| Required external libraries    |
#+--------------------------------+
import postgresql

#+--------------------------------------------------------------+
#| postgresql Threaded Connection                                    |
#+--------------------------------------------------------------+
class postgresql_connection(threading.Thread):
    def __init__(self,  _postgresqlhost, _postgresqlport, _username, _password):
        threading.Thread.__init__(self)
        self._postgresqlhost = _postgresqlhost
        self._postgresqlport = _postgresqlport
        self._username = _username
        self._password = _password
    def run(self):
        logging.info('Attempting to connect {0}@{1}:{2}'.format(self._username, self._postgresqlhost, self._postgresqlport))
        connection = postgresql.open(f'pq://{self._username}:{self._password}@{self._postgresqlhost}:{self._postgresqlport}/hlt_server')
        try:
            if connection.is_connected():
                db_Info = connection.get_server_info()
                logging.info("Connected to postgresql Server version {0}".format(db_Info))
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
                logging.info("postgresql connection is closed")
