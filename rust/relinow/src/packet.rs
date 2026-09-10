use relinow_sys::*;
use crate::error::Error;
use core::mem::MaybeUninit;

pub const HEADER_SIZE: usize = RELINOW_HEADER_SIZE as usize;

pub struct RelinowHeader {
    pub inner: relinow_header_t,
}

impl RelinowHeader {
    pub fn new(
        version: u8,
        mode: u8,
        type_: u8,
        flags: u8,
        seq_id: u16,
        ack_id: u16,
        channel_id: u8,
        payload_len: u16,
    ) -> Self {
        Self {
            inner: relinow_header_t {
                version,
                mode,
                type_,
                flags,
                seq_id,
                ack_id,
                channel_id,
                payload_len,
            },
        }
    }

    /// Encodes the header into a 9-byte buffer.
    pub fn encode(&self, max_payload: u16) -> Result<[u8; HEADER_SIZE], Error> {
        let mut buffer = [0u8; HEADER_SIZE];
        let err = unsafe {
            relinow_encode_header(&self.inner, max_payload, buffer.as_mut_ptr())
        };
        Error::into_result(err).map(|_| buffer)
    }

    /// Decodes a header from a raw frame byte slice.
    pub fn decode(frame: &[u8], max_payload: u16) -> Result<Self, Error> {
        let mut header = MaybeUninit::<relinow_header_t>::uninit();
        let err = unsafe {
            relinow_decode_header(
                frame.as_ptr(),
                frame.len(),
                max_payload,
                header.as_mut_ptr(),
            )
        };
        Error::into_result(err).map(|_| Self {
            inner: unsafe { header.assume_init() },
        })
    }

    /// Validates the header constraints.
    pub fn validate(&self, max_payload: u16) -> Result<(), Error> {
        let err = unsafe {
            relinow_validate_header(&self.inner, max_payload)
        };
        Error::into_result(err)
    }
}

pub fn is_seq_newer(seq_a: u16, seq_b: u16) -> bool {
    unsafe { relinow_is_seq_newer(seq_a, seq_b) != 0 }
}

pub fn seq_next(current: u16) -> u16 {
    unsafe { relinow_seq_next(current) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_decode() {
        let header = RelinowHeader::new(
            RELINOW_PROTOCOL_VERSION,
            RELINOW_MODE_RELIABLE,
            RELINOW_TYPE_DATA,
            0,
            1234,
            0,
            10,
            50,
        );

        let encoded = header.encode(250).unwrap();
        assert_eq!(encoded.len(), HEADER_SIZE);

        let mut frame = [0u8; HEADER_SIZE + 50];
        frame[..HEADER_SIZE].copy_from_slice(&encoded);
        
        let decoded = RelinowHeader::decode(&frame, 250).unwrap();
        assert_eq!(decoded.inner.seq_id, 1234);
        assert_eq!(decoded.inner.channel_id, 10);
        assert_eq!(decoded.inner.payload_len, 50);
    }
}
