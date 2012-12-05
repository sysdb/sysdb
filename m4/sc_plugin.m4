dnl Autoconf helper functions for the syscollector plugin handling.
dnl
dnl Copyright (C) 2005-2012 Florian 'octo' Forster <octo@verplant.org>
dnl Copyright (C) 2009-2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
dnl
dnl This program is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by the
dnl Free Software Foundation; only version 2 of the License is applicable.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

AC_DEFUN([AC_SC_PLUGIN_INIT],
	[
		dependency_error="no"
		dependency_warning="no"
		AC_ARG_ENABLE([all-plugins],
				AS_HELP_STRING([--enable-all-plugins],
						[enable all plugins (auto by default)]),
				[
				 if test "x$enableval" = "xyes"; then
					 enable_all_plugins="yes"
				 else if test "x$enableval" = "xauto"; then
					 enable_all_plugins="auto"
				 else
					 enable_all_plugins="no"
				 fi; fi
				],
				[enable_all_plugins="auto"]
		)
	]
)

dnl AC_SC_PLUGIN(name, default, info)
dnl
dnl Based on AC_PLUGIN of the collectd package.
AC_DEFUN([AC_SC_PLUGIN],
	[
		enable_plugin="no"
		force="no"
		AC_ARG_ENABLE([$1], AS_HELP_STRING([--enable-$1], [$3]),
			[
			 if test  "x$enableval" = "xyes"; then
				enable_plugin="yes"
			 else if test "x$enableval" = "xforce"; then
				enable_plugin="yes"
				force="yes"
			 else
				enable_plugin="no (disabled on command line)"
			 fi; fi
			],
			[
			 if test "x$enable_all_plugins" = "xauto"; then
				if test "x$2" = "xyes"; then
					enable_plugin="yes"
				else
					enable_plugin="no"
				fi
			 else
				enable_plugin="$enable_all_plugins"
			 fi
			]
		)
		if test "x$enable_plugin" = "xyes"; then
			if test "x$2" = "xyes" || test "x$force" = "xyes"; then
				AC_DEFINE([HAVE_PLUGIN_]m4_toupper([$1]), 1, [Define to 1 if the $1 plugin is enabled.])
				if test "x$2" != "xyes"; then
					dependency_warning="yes"
				fi
			else # User passed "yes" but dependency checking yielded "no" => Dependency problem.
				dependency_error="yes"
				enable_plugin="no (dependency error)"
			fi
		fi
		AM_CONDITIONAL([BUILD_PLUGIN_]m4_toupper([$1]), test "x$enable_plugin" = "xyes")
		enable_$1="$enable_plugin"
	]
)

