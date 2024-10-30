# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import REPLACE


def migrate(ctx):
    """Bug 1733498 - Convert necko.properties to Fluent, part {index}."""

    source = "netwerk/necko.properties"
    target = "netwerk/necko.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("network-connection-status-looking-up"),
                value=REPLACE(source, "3", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-connected"),
                value=REPLACE(source, "4", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-sending-request"),
                value=REPLACE(source, "5", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-transferring-data"),
                value=REPLACE(source, "6", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-connecting"),
                value=REPLACE(source, "7", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-read"),
                value=REPLACE(source, "8", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-wrote"),
                value=REPLACE(source, "9", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-waiting"),
                value=REPLACE(source, "10", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-looked-up"),
                value=REPLACE(source, "11", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-tls-handshake"),
                value=REPLACE(source, "12", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
            FTL.Message(
                id=FTL.Identifier("network-connection-status-tls-handshake-finished"),
                value=REPLACE(source, "13", {"%1$S": VARIABLE_REFERENCE("host")}),
            ),
        ],
    )
