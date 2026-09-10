use relinow_sys::*;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Error {
    InvalidArg,
    InvalidPacket,
    PayloadTooLarge,
    Unknown(i32),
}

impl Error {
    pub fn into_result(err: relinow_err_t) -> Result<(), Error> {
        match err {
            RELINOW_ERR_OK => Ok(()),
            RELINOW_ERR_INVALID_ARG => Err(Error::InvalidArg),
            RELINOW_ERR_INVALID_PACKET => Err(Error::InvalidPacket),
            RELINOW_ERR_PAYLOAD_TOO_LARGE => Err(Error::PayloadTooLarge),
            _ => Err(Error::Unknown(err)),
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum StateError {
    InvalidArg,
    NotFound,
    NoSpace,
    Conflict,
    QueueFull,
    WrongMode,
    Unknown(u32),
}

impl StateError {
    pub fn into_result(err: relinow_state_err_t) -> Result<(), StateError> {
        match err {
            RELINOW_STATE_OK => Ok(()),
            RELINOW_STATE_ERR_INVALID_ARG => Err(StateError::InvalidArg),
            RELINOW_STATE_ERR_NOT_FOUND => Err(StateError::NotFound),
            RELINOW_STATE_ERR_NO_SPACE => Err(StateError::NoSpace),
            RELINOW_STATE_ERR_CONFLICT => Err(StateError::Conflict),
            RELINOW_STATE_ERR_QUEUE_FULL => Err(StateError::QueueFull),
            RELINOW_STATE_ERR_WRONG_MODE => Err(StateError::WrongMode),
            _ => Err(StateError::Unknown(err)),
        }
    }
}
