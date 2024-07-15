/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useCallback, useRef } from "react";

function ModalOverlayWrapper({
  document = globalThis.document,
  unstyled,
  innerClassName,
  onClose,
  children,
  headerId,
  id,
}) {
  const modalRef = useRef(null);

  let className = unstyled ? "" : "modalOverlayInner active";
  if (innerClassName) {
    className += ` ${innerClassName}`;
  }

  // The intended behaviour is to listen for an escape key
  // but not for a click; see Bug 1582242
  const onKeyDown = useCallback(
    event => {
      if (event.key === "Escape") {
        onClose(event);
      }
    },
    [onClose]
  );

  useEffect(() => {
    document.addEventListener("keydown", onKeyDown);
    document.body.classList.add("modal-open");

    return () => {
      document.removeEventListener("keydown", onKeyDown);
      document.body.classList.remove("modal-open");
    };
  }, [document, onKeyDown]);

  return (
    <div
      className="modalOverlayOuter active"
      onKeyDown={onKeyDown}
      role="presentation"
    >
      <div
        className={className}
        aria-labelledby={headerId}
        id={id}
        role="dialog"
        ref={modalRef}
      >
        {children}
      </div>
    </div>
  );
}

export { ModalOverlayWrapper };
