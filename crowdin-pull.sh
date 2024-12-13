#!/bin/bash

# Add new languages here, space separated and using the ID for `crowdin pull`
LANGUAGES="de es-ES fr he hu id it ja kana nl pl ro ru ry tr uk zh-CN"

ARG=''
for LANGUAGE in $LANGUAGES; do
	ARG+="-l $LANGUAGE "
done
crowdin pull $ARG
