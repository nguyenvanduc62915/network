#ifndef STRUCTS_H
#define STRUCTS_H

enum Request {
    RequestNone,
    RequestSignIn,
    RequestSignUp,
    RequestSignOut,
    RequestGet,
    RequestCreateGroup,
    RequestJoinGroup,
    RequestCreateFolder,
    RequestUploadFile,
    RequestDownloadFile,
    RequestDelete,
};

enum Response {
    ResponseNone,
    ResponseSignInSuccess,
    ResponseSignInError,
    ResponseSignUpSuccess,
    ResponseSignUpError,
    ResponseSignOutSuccess,
    ResponseSignOutError,
    ResponseGetSuccess,
    ResponseGetError,
    ResponseCreateGroupSuccess,
    ResponseCreateGroupError,
    ResponseJoinGroupSuccess,
    ResponseJoinGroupError,
    ResponseCreateFolderSuccess,
    ResponseCreateFolderError,
    ResponseUploadFileSuccess,
    ResponseUploadFileError,
    ResponseDownloadFileSuccess,
    ResponseDownloadFileError,
    ResponseDeleteSuccess,
    ResponseDeleteError,
    ResponseSuccess,
    ResponseError,
};

#endif // STRUCTS_H
