[Constructor(DOMString type, optional MerchantValidationEventInit eventInitDict),
SecureContext,
Exposed=Window,
Func="mozilla::dom::PaymentRequest::PrefEnabled"]
interface MerchantValidationEvent : Event {
  readonly attribute USVString validationURL;
  [Throws]
  void complete(Promise<any> merchantSessionPromise);
};

dictionary MerchantValidationEventInit : EventInit {
  USVString validationURL = "";
};
