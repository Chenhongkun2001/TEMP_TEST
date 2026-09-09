#ifndef MISC_H
#define MISC_H

int Item_check_cb(void *NotUsed, int argc, char **argv, char **azColName);
int gateway_cb(void *NotUsed, int argc, char **argv, char **azColName);
int devicelist_cb(void *NotUsed, int argc, char **argv, char **azColName);
int sensor_cb(void *NotUsed, int argc, char **argv, char **azColName);
int sensordata_cb(void *NotUsed, int argc, char **argv, char **azColName);
int autoid_cb(void *NotUsed, int argc, char **argv, char **azColName);
int fliter_cb(void *NotUsed, int argc, char **argv, char **azColName);
#endif
