use super::{Command, CommandError, PinUvAuthCommand, RequestCtap2, StatusCode};
use crate::{
    crypto::{PinUvAuthParam, PinUvAuthToken},
    ctap2::server::UserVerificationRequirement,
    errors::AuthenticatorError,
    transport::errors::HIDError,
    AuthenticatorInfo, FidoDevice,
};
use serde::{Deserialize, Serialize, Serializer};
use serde_cbor::{de::from_slice, to_vec, Value};

#[derive(Debug, Clone, Deserialize)]
pub struct SetMinPINLength {
    /// Minimum PIN length in code points
    pub new_min_pin_length: Option<u64>,
    /// RP IDs which are allowed to get this information via the minPinLength extension.
    /// This parameter MUST NOT be used unless the minPinLength extension is supported.  
    pub min_pin_length_rpids: Option<Vec<String>>,
    /// The authenticator returns CTAP2_ERR_PIN_POLICY_VIOLATION until changePIN is successful.    
    pub force_change_pin: Option<bool>,
}

impl Serialize for SetMinPINLength {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serialize_map_optional!(
            serializer,
            &0x01 => self.new_min_pin_length,
            &0x02 => &self.min_pin_length_rpids,
            &0x03 => self.force_change_pin,
        )
    }
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub enum AuthConfigCommand {
    EnableEnterpriseAttestation,
    ToggleAlwaysUv,
    SetMinPINLength(SetMinPINLength),
}

impl AuthConfigCommand {
    fn has_params(&self) -> bool {
        match self {
            AuthConfigCommand::EnableEnterpriseAttestation => false,
            AuthConfigCommand::ToggleAlwaysUv => false,
            AuthConfigCommand::SetMinPINLength(..) => true,
        }
    }
}

#[derive(Debug, Serialize)]
pub enum AuthConfigResult {
    Success(AuthenticatorInfo),
}

#[derive(Debug)]
pub struct AuthenticatorConfig {
    subcommand: AuthConfigCommand, // subCommand currently being requested
    pin_uv_auth_param: Option<PinUvAuthParam>, // First 16 bytes of HMAC-SHA-256 of contents using pinUvAuthToken.
}

impl AuthenticatorConfig {
    pub(crate) fn new(subcommand: AuthConfigCommand) -> Self {
        Self {
            subcommand,
            pin_uv_auth_param: None,
        }
    }
}

impl Serialize for AuthenticatorConfig {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let (entry01, entry02) = match &self.subcommand {
            AuthConfigCommand::EnableEnterpriseAttestation => (&0x01, None),
            AuthConfigCommand::ToggleAlwaysUv => (&0x02, None),
            AuthConfigCommand::SetMinPINLength(params) => (&0x03, Some(params)),
        };

        serialize_map_optional!(
            serializer,
            &0x01 => Some(entry01),
            &0x02 => entry02,
            &0x03 => self.pin_uv_auth_param.as_ref().map(|p| p.pin_protocol.id()),
            &0x04 => &self.pin_uv_auth_param,
        )
    }
}

impl RequestCtap2 for AuthenticatorConfig {
    type Output = ();

    fn command(&self) -> Command {
        Command::AuthenticatorConfig
    }

    fn wire_format(&self) -> Result<Vec<u8>, HIDError> {
        let output = to_vec(&self).map_err(CommandError::Serializing)?;
        trace!("client subcommmand: {:04X?}", &output);
        Ok(output)
    }

    fn handle_response_ctap2<Dev>(
        &self,
        _dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError>
    where
        Dev: FidoDevice,
    {
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();

        if status.is_ok() {
            Ok(())
        } else {
            let msg = if input.len() > 1 {
                let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Some(data)
            } else {
                None
            };
            Err(CommandError::StatusCode(status, msg).into())
        }
    }

    fn send_to_virtual_device<Dev: crate::VirtualFidoDevice>(
        &self,
        _dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        unimplemented!()
    }
}

impl PinUvAuthCommand for AuthenticatorConfig {
    fn set_pin_uv_auth_param(
        &mut self,
        pin_uv_auth_token: Option<PinUvAuthToken>,
    ) -> Result<(), AuthenticatorError> {
        let mut param = None;
        if let Some(token) = pin_uv_auth_token {
            // pinUvAuthParam (0x04): the result of calling
            // authenticate(pinUvAuthToken, 32×0xff || 0x0d || uint8(subCommand) || subCommandParams).
            let mut data = vec![0xff; 32];
            data.push(0x0D);
            match &self.subcommand {
                AuthConfigCommand::EnableEnterpriseAttestation => {
                    data.push(0x01);
                }
                AuthConfigCommand::ToggleAlwaysUv => {
                    data.push(0x02);
                }
                AuthConfigCommand::SetMinPINLength(params) => {
                    data.push(0x03);
                    data.extend(to_vec(params).map_err(CommandError::Serializing)?);
                }
            }
            param = Some(token.derive(&data).map_err(CommandError::Crypto)?);
        }
        self.pin_uv_auth_param = param;
        Ok(())
    }

    fn can_skip_user_verification(
        &mut self,
        authinfo: &AuthenticatorInfo,
        _uv_req: UserVerificationRequirement,
    ) -> bool {
        !authinfo.device_is_protected()
    }

    fn set_uv_option(&mut self, _uv: Option<bool>) {
        /* No-op */
    }

    fn get_pin_uv_auth_param(&self) -> Option<&PinUvAuthParam> {
        self.pin_uv_auth_param.as_ref()
    }

    fn get_rp_id(&self) -> Option<&String> {
        None
    }
}

#[cfg(test)]
mod test {
    use crate::{crypto::PinUvAuthParam, ctap2::commands::client_pin::PinUvAuthTokenPermission};

    use super::{AuthConfigCommand, AuthenticatorConfig, SetMinPINLength};

    #[test]
    fn test_serialize_set_min_pin_length() {
        let set_min_pin_length = SetMinPINLength {
            new_min_pin_length: Some(42),
            min_pin_length_rpids: Some(vec![
                "example.org".to_string(),
                "www.example.org".to_string(),
            ]),
            force_change_pin: Some(true),
        };
        let serialized =
            serde_cbor::ser::to_vec(&set_min_pin_length).expect("Failed to serialize to CBOR");
        assert_eq!(
            serialized,
            [
                // Value copied from test failure output as regression test snapshot
                163, 1, 24, 42, 2, 130, 107, 101, 120, 97, 109, 112, 108, 101, 46, 111, 114, 103,
                111, 119, 119, 119, 46, 101, 120, 97, 109, 112, 108, 101, 46, 111, 114, 103, 3,
                245
            ]
        );
    }

    #[test]
    fn test_serialize_authenticator_config() {
        let pin_uv_auth_param = Some(PinUvAuthParam::create_test(
            2,
            vec![1, 2, 3, 4],
            PinUvAuthTokenPermission::CredentialManagement,
        ));

        {
            let authenticator_config = AuthenticatorConfig {
                subcommand: AuthConfigCommand::EnableEnterpriseAttestation,
                pin_uv_auth_param: pin_uv_auth_param.clone(),
            };
            let serialized = serde_cbor::ser::to_vec(&authenticator_config)
                .expect("Failed to serialize to CBOR");
            assert_eq!(
                serialized,
                [
                    // Value copied from test failure output as regression test snapshot
                    163, 1, 1, 3, 2, 4, 68, 1, 2, 3, 4
                ]
            );
        }

        {
            let authenticator_config = AuthenticatorConfig {
                subcommand: AuthConfigCommand::ToggleAlwaysUv,
                pin_uv_auth_param: pin_uv_auth_param.clone(),
            };
            let serialized = serde_cbor::ser::to_vec(&authenticator_config)
                .expect("Failed to serialize to CBOR");
            assert_eq!(
                serialized,
                [
                    // Value copied from test failure output as regression test snapshot
                    163, 1, 2, 3, 2, 4, 68, 1, 2, 3, 4
                ]
            );
        }

        {
            let authenticator_config = AuthenticatorConfig {
                subcommand: AuthConfigCommand::SetMinPINLength(SetMinPINLength {
                    new_min_pin_length: Some(42),
                    min_pin_length_rpids: Some(vec![
                        "example.org".to_string(),
                        "www.example.org".to_string(),
                    ]),
                    force_change_pin: Some(true),
                }),
                pin_uv_auth_param,
            };
            let serialized = serde_cbor::ser::to_vec(&authenticator_config)
                .expect("Failed to serialize to CBOR");
            assert_eq!(
                serialized,
                [
                    // Value copied from test failure output as regression test snapshot
                    164, 1, 3, 2, 163, 1, 24, 42, 2, 130, 107, 101, 120, 97, 109, 112, 108, 101, 46,
                    111, 114, 103, 111, 119, 119, 119, 46, 101, 120, 97, 109, 112, 108, 101, 46,
                    111, 114, 103, 3, 245, 3, 2, 4, 68, 1, 2, 3, 4
                ]
            );
        }
    }
}
