/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2008-2017 MonetDB B.V.
 */

#include "nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection.h"

#include "javaids.h"
#include "jresulset.h"
#include "embedded.h"
#include "monetdb_config.h"
#include "res_table.h"
#include "gdk.h"

static void setErrorResponse(JNIEnv *env, jobject jdbccon, char* errorMessage) {
    jintArray lineResponse = (jintArray) (*env)->GetObjectField(env, jdbccon, getServerResponsesID());
    jint response[2] = {1,4}; //ERROR AND PROMPT
    (*env)->SetIntArrayRegion(env, lineResponse, 0, 2, response);
    (*env)->SetObjectField(env, jdbccon, getLastErrorID(), (*env)->NewStringUTF(env, errorMessage));
    GDKfree(errorMessage);
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_getNextTableHeaderInternal
    (JNIEnv *env, jobject jdbccon, jlong resultSetPointer, jobjectArray columnNames, jintArray columnLengths,
    jobjectArray types, jobjectArray tableNames) {
    res_table *output = (res_table *) resultSetPointer;
    int numberOfColumns = (*env)->GetArrayLength(env, columnNames);
    jint* columnLengthsFound;
    res_col col;
    jstring colname, sqlname, tablename;
    (void) jdbccon;

    columnLengthsFound = GDKmalloc(numberOfColumns * sizeof(jint));
    if(columnLengthsFound == NULL) {
        (*env)->ThrowNew(env, getMonetDBEmbeddedExceptionClassID(), "System out of memory!");
        return;
    }

    for (int i = 0; i < numberOfColumns; i++) {
        col = output->cols[i];
        columnLengthsFound[i] = col.type.digits;
        colname = (*env)->NewStringUTF(env, col.name);
        sqlname = (*env)->NewStringUTF(env, col.type.type->sqlname);
        tablename = (*env)->NewStringUTF(env, col.tn);
        (*env)->SetObjectArrayElement(env, columnNames, i, colname);
        (*env)->SetObjectArrayElement(env, types, i, sqlname);
        (*env)->SetObjectArrayElement(env, tableNames, i, tablename);
        (*env)->DeleteLocalRef(env, colname);
        (*env)->DeleteLocalRef(env, sqlname);
        (*env)->DeleteLocalRef(env, tablename);
    }
    (*env)->SetIntArrayRegion(env, columnLengths, 0, numberOfColumns, columnLengthsFound);
    GDKfree(columnLengthsFound);
    //Important! Don't free the result table yet!
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_initializePointersInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jlong lastResultSetPointer,
        jobject embeddedDataBlockResponse) {
    res_table* output = (res_table*) lastResultSetPointer;
    JResultSet* thisResultSet;
    (void) jdbccon;

    thisResultSet = createResultSet((Client) connectionPointer, output);
    if(thisResultSet == NULL) {
        (*env)->ThrowNew(env, getMonetDBEmbeddedExceptionClassID(), "Connection already closed?");
    }
    (*env)->SetLongField(env, embeddedDataBlockResponse, getStructPointerID(), (jlong) thisResultSet);
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_sendQueryInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jstring query, jboolean execute) {
    long lastId, rowCount;
    int lineResponseCounter = 0, query_type, autoCommitStatus, numberOfRows;
    jint nextResponses[4], responseParameters[3];
    const char *query_string_tmp;
    char *err = NULL;
    res_table *output = NULL;
    BAT* dearBat;
    jintArray lineResponse, lastServerResponseParameters;
    Client conn = (Client) connectionPointer;

    if(conn == NULL) {
        (*env)->ThrowNew(env, getMonetDBEmbeddedExceptionClassID(), "Connection already closed?");
        return;
    }

    query_string_tmp = (*env)->GetStringUTFChars(env, query, NULL);
    if(query_string_tmp == NULL) {
        (*env)->ThrowNew(env, getMonetDBEmbeddedExceptionClassID(), "System out of memory!");
        return;
    }

    err = monetdb_query(conn, (char*) query_string_tmp, (char) execute, (void**) &output);
    query_type = conn->lastQueryType;
    (*env)->ReleaseStringUTFChars(env, query, query_string_tmp);
    if (err) { //if there are errors, set the error string and exit
        setErrorResponse(env, jdbccon, err);
        monetdb_cleanup_result(conn, output);
        return;
    }

    //set the result set pointer
    (*env)->SetLongField(env, jdbccon, getLastResultSetPointerID(), (jlong) output);
    nextResponses[lineResponseCounter++] = 6; //SOHEADER

    //set the next serverHeader
    (*env)->SetIntField(env, jdbccon, getServerHeaderResponseID(), query_type);

    // The SCHEMA responses (query_type == Q_SCHEMA), don't need anything from the server
    switch(query_type) {
        case Q_TABLE: //TABLE
        case Q_PREPARE: //PREPARE
        case Q_BLOCK: //BLOCK
            //set the Table Headers values
            if(output) {
                dearBat = BATdescriptor(output->cols[0].b);
                numberOfRows = BATcount(dearBat);
                BBPunfix(dearBat->batCacheid);
                responseParameters[1] = numberOfRows; //number of rows
            } else {
                responseParameters[1] = 0;
            }
            responseParameters[0] = (output) ? output->id : -1;
            if(query_type == Q_TABLE || query_type == Q_PREPARE) {
                responseParameters[2] = (output) ? output->nr_cols: 0; //number of columns
            }
            //set the other headers
            nextResponses[lineResponseCounter++] = 2; //HEADER
            //IMPORTANT Due to the Embedded architecture, we can skip the RESULT header in the response
            lastServerResponseParameters = (jintArray) (*env)->GetObjectField(env, jdbccon, getLastServerResponseParametersID());
            (*env)->SetIntArrayRegion(env, lastServerResponseParameters, 0, 3, responseParameters);
            break;
        case Q_UPDATE: //UPDATE
            getUpdateQueryData(conn, &lastId, &rowCount);
            (*env)->SetObjectField(env, jdbccon, getLastServerResponseID(), (*env)->NewObject(env,
            getUpdateResponseClassID(), getUpdateResponseConstructorID(), (jint) lastId, (jint) rowCount));
            break;
        case Q_TRANS: //TRANSACTION
            autoCommitStatus = getAutocommitFlag(conn);
            (*env)->SetObjectField(env, jdbccon, getLastServerResponseID(), (*env)->NewObject(env,
            getAutoCommitResponseClassID(), getAutoCommitResponseConstructorID(), (autoCommitStatus) ? JNI_TRUE : JNI_FALSE));
            break;
    }
    nextResponses[lineResponseCounter++] = 4; //PROMPT
    //set the line response headers
    lineResponse = (jintArray) (*env)->GetObjectField(env, jdbccon, getServerResponsesID());
    (*env)->SetIntArrayRegion(env, lineResponse, 0, lineResponseCounter, nextResponses);

    if(query_type != Q_TABLE && query_type != Q_PREPARE && output) { //if the result is not a table or a prepare, delete it right away
        monetdb_cleanup_result(conn, output);
    }
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_sendAutocommitCommandInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jint flag) {
    int autoCommitStatus;

    char *err = sendAutoCommitCommand((Client) connectionPointer, flag, &autoCommitStatus);
    if (err) { //if there is an error set it and return
        setErrorResponse(env, jdbccon, err);
    } else {
        (*env)->SetObjectField(env, jdbccon, getLastServerResponseID(), (*env)->NewObject(env,
        getAutoCommitResponseClassID(), getAutoCommitResponseConstructorID(), (autoCommitStatus) ? JNI_TRUE : JNI_FALSE));
    }
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_sendReplySizeCommandInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jint size) {
    (void) env;
    (void) jdbccon;
    sendReplySizeCommand((Client) connectionPointer, (long) size);
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_sendReleaseCommandInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jint commandId) {
    (void) env;
    (void) jdbccon;
    sendReleaseCommand((Client) connectionPointer, commandId);
}

JNIEXPORT void JNICALL Java_nl_cwi_monetdb_embedded_jdbc_JDBCEmbeddedConnection_sendCloseCommandInternal
    (JNIEnv *env, jobject jdbccon, jlong connectionPointer, jint commandId) {
    (void) env;
    (void) jdbccon;
    sendCloseCommand((Client) connectionPointer, commandId);
}
