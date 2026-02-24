#!/bin/bash

echo
echo "Warning, this is slow and very inefficient"
echo

cd "$(git rev-parse --show-toplevel)"

files=( $(find libraries/Translation -name en_US.po) )
folders=( *src* )

wadstrings=
if [[ -n $DOOMWADDIR ]]
then
	echo "collecting WADs"
	echo

	pushd "${DOOMWADDIR}" >/dev/null
	wads=( ./* )
	i=0
	for file in "${wads[@]}"
	do
		((i++));
		printf "%3d/%d %s\n" "$i" "${#wads[@]}" "$(md5sum "$file")"
		_wadstrings="$( \
			strings -n 1 "$file" \
				| grep -E '^[A-Z][a-zA-Z_0-9]{0,35}$' \
				| grep -Ev '^TXT_' \
				| sort -u \
		)"
		wc -l <<<"$_wadstrings"
		wadstrings="$(cat <<<"$wadstrings" <<<"$_wadstrings" | sort -u )"
	done
	popd >/dev/null
	echo
	printf "indexed %s strings\n" "$(wc -l <<<"$wadstrings")"
	echo
fi

hotfiles=()
f_index=0
for file in "${files[@]}"
do
	((f_index++));

	printf "%3d/%d %s\n" ${f_index} ${#files[@]} "${file}"

	[[ -f "${file}" ]] || continue
	for token in $(grep msgid "${file}" | cut -d \" -f 2 | sort -u)
	do
		[[ -z "${token}" ]] && continue
		[[ "${token}" == TXT_* ]] && continue # just assume these are fine

		inwad=0
		insource=0
		found=0

 		if grep -qF "${token}" <<< "${wadstrings}"
 		then
			inwad=1
		fi

		for file in "${hotfiles[@]}"
		do
			grep -Fqm 1 "${token}" "${file}" || continue
			insource=1
			break
		done

		[[ "${insource}" == 0 ]] && for folder in "${folders[@]}"
		do
			[[ -d "${folder}" ]] || continue

			match=$(grep -IrlFm 1 "${token}" "${folder}" | head -1)
			[[ -z "${match}" ]] && continue
			hotfiles+=("${match}")
			insource=1

			break
		done

		[[ "${insource}" == 0 ]] && [[ "${inwad}" == 0 ]] && printf "\t%s\n" "${token}"
# 		[[ "${insource}" == 1 ]] && [[ "${inwad}" == 0 ]] && printf "s\t%s\n" "${token}"
		[[ "${insource}" == 0 ]] && [[ "${inwad}" == 1 ]] && printf "w\t%s\n" "${token}"
	done
	echo
done
