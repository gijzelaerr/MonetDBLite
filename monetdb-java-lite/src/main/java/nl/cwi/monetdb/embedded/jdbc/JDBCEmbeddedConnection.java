/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2017 MonetDB B.V.
 */

package nl.cwi.monetdb.embedded.jdbc;

import nl.cwi.monetdb.embedded.env.MonetDBEmbeddedConnection;
import nl.cwi.monetdb.embedded.env.MonetDBEmbeddedDatabase;
import nl.cwi.monetdb.embedded.env.MonetDBEmbeddedException;
import nl.cwi.monetdb.mcl.protocol.ServerResponses;
import nl.cwi.monetdb.mcl.protocol.StarterHeaders;
import nl.cwi.monetdb.mcl.protocol.TableResultHeaders;
import nl.cwi.monetdb.mcl.responses.*;

/**
 * An extension to the {@link MonetDBEmbeddedConnection} in order to adapt it into a MonetDB's JDBC connection. This
 * class is not Thread safe and provides JNI calls to perform actions on the server. After a user query is sent,
 * the fields of this class are filled immediately upon by the JNI calls. After that, these fields provide enough
 * information to mimic a MAPI connection in {@link nl.cwi.monetdb.jdbc.MonetConnection.ResponseList} main loop.
 *
 * @author <a href="mailto:pedro.ferreira@monetdbsolutions.com">Pedro Ferreira</a>
 */
public final class JDBCEmbeddedConnection extends MonetDBEmbeddedConnection {

    /**
     * The C pointer to the last ResultSet from a query generated by MonetDB. To be used by the JNI calls.
     */
    private volatile long lastResultSetPointer;

    /**
     * An array to hold {@link ServerResponses} values in a response.
     */
    private final int[] lineResponse = new int[4];

    /**
     * The iterator for the lineResponse array during the response retrieval.
     */
    private volatile int currentLineResponseState;

    /**
     * Holds the {@link StarterHeaders} value for a {@link ResultSetResponse}.
     */
    private volatile int serverHeaderResponse;

    /**
     * For a {@link ResultSetResponse}, this array gets filled with the block's metadata: query_id, numberOfRows and
     * numberOfColumns (the number of tuples is always equal to the number of rows) in an embedded connection.
     */
    private final int[] lastServerResponseParameters = new int[4];

    /**
     * Holds {@link UpdateResponse} or {@link AutoCommitResponse} responses instances when they happen.
     */
    private IResponse lastServerResponse;

    /**
     * When errors happen in the server, this String holds the message.
     */
    private String lastError;

    protected JDBCEmbeddedConnection(long connectionPointer) {
        super(connectionPointer);
    }

    @Override
    public void close() {
        if(!this.isClosed()) {
            this.closeConnectionImplementation();
            MonetDBEmbeddedDatabase.removeConnection(this, true);
        }
    }

    /**
     * Gets the current server response, obtained immediately after a query is performed.
     *
     * @return The integer representation of {@link ServerResponses}
     */
    int getNextServerResponse() {
        return lineResponse[currentLineResponseState++];
    }

    /**
     * Gets the next starter header of a server response.
     *
     * @return The integer representation of {@link StarterHeaders}
     */
    int getServerHeaderResponse() {
        return serverHeaderResponse;
    }

    /**
     * Gets the block's metadata information for a {@link ResultSetResponse}
     *
     * @return The block's query_id, numberOfRows and numberOfColumns
     */
    int[] getLastServerResponseParameters() {
        return lastServerResponseParameters;
    }

    /**
     * Returns {@link UpdateResponse} or {@link AutoCommitResponse} responses instances when they happen.
     *
     * @return A {@link UpdateResponse} or {@link AutoCommitResponse}
     */
    IResponse getLastServerResponse() {
        return lastServerResponse;
    }

    /**
     * Fills a {@link ResultSetResponse} table data.
     *
     * @param columnNames The column names array
     * @param columnLengths The column lengths array
     * @param types The columns SQL names array
     * @param tableNames The columns schemas and names in format schema.table
     * @return Always TableResultHeaders.ALL
     */
    int fillTableHeaders(String[] columnNames, int[] columnLengths, String[] types, String[] tableNames) {
        this.getNextTableHeaderInternal(this.lastResultSetPointer, columnNames, columnLengths, types, tableNames);
        return TableResultHeaders.ALL;
    }

    /**
     * Fills the result set data for a {@link AbstractDataBlockResponse} according into the JDBC mappings.
     *
     * @param response The response to set data
     */
    void initializePointers(EmbeddedDataBlockResponse response) throws MonetDBEmbeddedException {
        this.initializePointersInternal(this.connectionPointer, this.lastResultSetPointer, response);
    }

    /**
     * Retrieves the server's last error String.
     *
     * @return The error as a String
     */
    String getLastError() {
        return lastError;
    }

    /**
     * Provides a user query to the server, executes it and fills the fields of this class immediately upon.
     *
     * @param query The user query to submit to the server
     */
    void processNextQuery(String query) {
        if (!query.endsWith(";")) {
            query += ";";
        }
        this.currentLineResponseState = 0; //Important reset the currentLineResponseState back to 0!!!!
        this.sendQueryInternal(this.connectionPointer, query, true);
    }

    /**
     * Sends an autocommit command to the server.
     *
     * @param flag If 0 turn the autocommit mode off, on otherwise
     */
    void sendAutocommitCommand(int flag) { //1 or 0
        this.sendAutocommitCommandInternal(this.connectionPointer, flag);
    }

    /**
     * Sends a reply size command to the server. (The reply size on an embedded connection is always fixed, hence this
     * method is here for completeness).
     *
     * @param size The reply size value to set
     */
    void sendReplySizeCommand(int size) {
        this.sendReplySizeCommandInternal(this.connectionPointer, size);
    }

    /**
     * Sends a release command to the server. (Used to release a prepared statement data, altough is not yet implemented
     * in an embedded connection, the command is here for future usage).
     *
     * @param resultSetId The prepared statement id
     */
    void sendReleaseCommand(int resultSetId) {
        this.sendReleaseCommandInternal(this.connectionPointer, resultSetId);
    }

    /**
     * Sends a release command to the server to close a query.
     *
     * @param queryId The query's id
     */
    void sendCloseCommand(int queryId) {
        this.sendCloseCommandInternal(this.connectionPointer, queryId);
    }

    /**
     * Native implementation of table headers retrieval.
     */
    private native void getNextTableHeaderInternal(long resultSetPointer, String[] columnNames, int[] columnLengths,
                                                   String[] types, String[] tableNames);

    /**
     * Native implementation of the result set construction.
     */
    private native void initializePointersInternal(long connectionPointer, long resultSetPointer,
                                                   EmbeddedDataBlockResponse response)
            throws MonetDBEmbeddedException;

    /**
     * Native implementation of query execution.
     */
    private native void sendQueryInternal(long connectionPointer, String query, boolean execute);

    /**
     * Native implementation of the autocommit command.
     */
    private native void sendAutocommitCommandInternal(long connectionPointer, int flag);

    /**
     * Native implementation of the reply size command.
     */
    private native void sendReplySizeCommandInternal(long connectionPointer, int size);

    /**
     * Native implementation of the release command.
     */
    private native void sendReleaseCommandInternal(long connectionPointer, int commandId);

    /**
     * Native implementation of the close command.
     */
    private native void sendCloseCommandInternal(long connectionPointer, int commandId);
}
